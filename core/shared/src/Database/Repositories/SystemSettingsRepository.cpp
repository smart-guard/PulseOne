// =============================================================================
// core/shared/src/Database/Repositories/SystemSettingsRepository.cpp
// 인라인 SQL → SQL::SystemSettings:: 헤더 상수 참조로 교체
// 🔧 NOW() 버그 수정: datetime('now', 'localtime') → adaptQuery()가 DB별 변환
// =============================================================================

#include "Database/Repositories/SystemSettingsRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "DatabaseAbstractionLayer.hpp"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// SQL 네임스페이스 단축
namespace SS = PulseOne::Database::SQL::SystemSettings;

SystemSettingsRepository::SystemSettingsRepository()
    : IRepository<SystemSettingsEntity>("SystemSettingsRepository") {}

// -----------------------------------------------------------------------------
// findAll
// -----------------------------------------------------------------------------
std::vector<SystemSettingsEntity> SystemSettingsRepository::findAll() {
  try {
    DbLib::DatabaseAbstractionLayer db;
    // ✅ SQL::SystemSettings::FIND_ALL
    auto results = db.executeQuery(SS::FIND_ALL);
    return mapResultToEntities(results);
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::findAll failed: " +
                   std::string(e.what()));
    return {};
  }
}

// -----------------------------------------------------------------------------
// findById
// -----------------------------------------------------------------------------
std::optional<SystemSettingsEntity> SystemSettingsRepository::findById(int id) {
  try {
    DbLib::DatabaseAbstractionLayer db;
    // ✅ SQL::SystemSettings::FIND_BY_ID
    auto results = db.executeQuery(SS::FIND_BY_ID(id));
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
    // ✅ SQL::SystemSettings::FIND_BY_KEY
    auto results = db.executeQuery(SS::FIND_BY_KEY(key));
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
// 🔧 NOW() → datetime('now', 'localtime') 수정
// -----------------------------------------------------------------------------
bool SystemSettingsRepository::updateValue(const std::string &key,
                                           const std::string &value) {
  try {
    DbLib::DatabaseAbstractionLayer db;

    auto existing = findByKey(key);

    if (existing.has_value()) {
      // ✅ SQL::SystemSettings::UPDATE_VALUE (NOW() 버그 수정됨)
      return db.executeNonQuery(SS::UPDATE_VALUE(key, value));
    } else {
      // ✅ SQL::SystemSettings::INSERT_VALUE
      return db.executeNonQuery(SS::INSERT_VALUE(key, value));
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
    // ✅ SQL::SystemSettings::DELETE_BY_ID
    return db.executeNonQuery(SS::DELETE_BY_ID(id));
  } catch (const std::exception &e) {
    logger_->Error("SystemSettingsRepository::deleteById failed: " +
                   std::string(e.what()));
    return false;
  }
}

bool SystemSettingsRepository::exists(int id) {
  try {
    DbLib::DatabaseAbstractionLayer db;
    // ✅ SQL::SystemSettings::COUNT_BY_ID
    auto results = db.executeQuery(SS::COUNT_BY_ID(id));
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
// mapRowToEntity / mapResultToEntities
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
