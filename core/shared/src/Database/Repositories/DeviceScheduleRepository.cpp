// =============================================================================
// core/shared/src/Database/Repositories/DeviceScheduleRepository.cpp
// 인라인 SQL → SQL::DeviceSchedule:: 헤더 상수 참조로 교체
// =============================================================================

#include "Database/Repositories/DeviceScheduleRepository.h"
#include "Database/Repositories/RepositoryHelpers.h"
#include "Database/SQLQueries.h"
#include "DatabaseAbstractionLayer.hpp"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

// SQL 네임스페이스 단축
namespace DS = PulseOne::Database::SQL::DeviceSchedule;

DeviceScheduleEntity DeviceScheduleRepository::mapRowToEntity(
    const std::map<std::string, std::string> &row) {
  DeviceScheduleEntity entity(std::stoi(row.at("id")));
  entity.setDeviceId(std::stoi(row.at("device_id")));
  if (row.at("point_id") != "" && row.at("point_id") != "NULL") {
    entity.setPointId(std::stoi(row.at("point_id")));
  }
  entity.setScheduleType(row.at("schedule_type"));
  entity.setScheduleData(row.at("schedule_data"));
  entity.setSynced(row.at("is_synced") == "1");

  if (row.count("last_sync_time") && !row.at("last_sync_time").empty() &&
      row.at("last_sync_time") != "NULL") {
    // Simple string parsing for SQLite datetime
    // Format: YYYY-MM-DD HH:MM:SS
    std::string time_str = row.at("last_sync_time");
    if (time_str.length() >= 19) {
      std::tm tm = {};
      std::istringstream ss(time_str);
      ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
      if (!ss.fail()) {
        auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
        entity.setLastSyncTime(tp);
      }
    }
  }
  return entity;
}

DeviceScheduleRepository::DeviceScheduleRepository()
    : IRepository<DeviceScheduleEntity>("DeviceScheduleRepository") {}

std::vector<DeviceScheduleEntity> DeviceScheduleRepository::findAll() {
  DbLib::DatabaseAbstractionLayer db;
  // ✅ SQL::DeviceSchedule::FIND_ALL
  auto results = db.executeQuery(DS::FIND_ALL);

  std::vector<DeviceScheduleEntity> entities;
  for (const auto &row : results) {
    entities.push_back(mapRowToEntity(row));
  }
  return entities;
}

std::optional<DeviceScheduleEntity> DeviceScheduleRepository::findById(int id) {
  DbLib::DatabaseAbstractionLayer db;
  // ✅ SQL::DeviceSchedule::FIND_BY_ID
  auto results = db.executeQuery(DS::FIND_BY_ID(id));
  if (results.empty())
    return std::nullopt;
  return mapRowToEntity(results[0]);
}

bool DeviceScheduleRepository::save(DeviceScheduleEntity &entity) {
  DbLib::DatabaseAbstractionLayer db;
  // ✅ SQL::DeviceSchedule::INSERT
  std::string point_id_or_null =
      entity.getPointId().has_value()
          ? std::to_string(entity.getPointId().value())
          : "NULL";
  std::string sql = DS::INSERT(entity.getDeviceId(), point_id_or_null,
                               entity.getScheduleType(),
                               entity.getScheduleData(), entity.isSynced());

  if (db.executeNonQuery(sql)) {
    // DB 타입별 자동 분기 (SQLite/PG/MySQL/MariaDB/MSSQL)
    int64_t new_id = db.getLastInsertId();
    if (new_id > 0) {
      entity.setId(static_cast<int>(new_id));
      entity.markSaved();
      return true;
    }
  }
  return false;
}

bool DeviceScheduleRepository::update(const DeviceScheduleEntity &entity) {
  DbLib::DatabaseAbstractionLayer db;
  // ✅ SQL::DeviceSchedule::UPDATE
  std::string point_id_or_null =
      entity.getPointId().has_value()
          ? std::to_string(entity.getPointId().value())
          : "NULL";
  std::string sql = DS::UPDATE(entity.getId(), entity.getDeviceId(),
                               point_id_or_null, entity.getScheduleType(),
                               entity.getScheduleData(), entity.isSynced());
  return db.executeNonQuery(sql);
}

bool DeviceScheduleRepository::deleteById(int id) {
  DbLib::DatabaseAbstractionLayer db;
  // ✅ SQL::DeviceSchedule::DELETE_BY_ID
  return db.executeNonQuery(DS::DELETE_BY_ID(id));
}

bool DeviceScheduleRepository::exists(int id) {
  return findById(id).has_value();
}

std::vector<DeviceScheduleEntity>
DeviceScheduleRepository::findByDeviceId(int device_id) {
  DbLib::DatabaseAbstractionLayer db;
  // ✅ SQL::DeviceSchedule::FIND_BY_DEVICE_ID
  auto results = db.executeQuery(DS::FIND_BY_DEVICE_ID(device_id));
  std::vector<DeviceScheduleEntity> entities;
  for (const auto &row : results) {
    entities.push_back(mapRowToEntity(row));
  }
  return entities;
}

std::vector<DeviceScheduleEntity>
DeviceScheduleRepository::findPendingSync(int device_id) {
  DbLib::DatabaseAbstractionLayer db;
  // ✅ SQL::DeviceSchedule::FIND_PENDING_SYNC / FIND_PENDING_BY_DEVICE
  std::string sql = (device_id != -1) ? DS::FIND_PENDING_BY_DEVICE(device_id)
                                      : DS::FIND_PENDING_SYNC;
  auto results = db.executeQuery(sql);
  std::vector<DeviceScheduleEntity> entities;
  for (const auto &row : results) {
    entities.push_back(mapRowToEntity(row));
  }
  return entities;
}

bool DeviceScheduleRepository::markAsSynced(int id) {
  DbLib::DatabaseAbstractionLayer db;
  // ✅ SQL::DeviceSchedule::MARK_SYNCED
  return db.executeNonQuery(DS::MARK_SYNCED(id));
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne
