#include "Database/Repositories/DeviceScheduleRepository.h"
#include "DatabaseAbstractionLayer.hpp"
#include "Database/Repositories/RepositoryHelpers.h"
#include <sstream>

namespace PulseOne {
namespace Database {
namespace Repositories {

DeviceScheduleEntity DeviceScheduleRepository::mapRowToEntity(const std::map<std::string, std::string>& row) {
    DeviceScheduleEntity entity(std::stoi(row.at("id")));
    entity.setDeviceId(std::stoi(row.at("device_id")));
    if (row.at("point_id") != "" && row.at("point_id") != "NULL") {
        entity.setPointId(std::stoi(row.at("point_id")));
    }
    entity.setScheduleType(row.at("schedule_type"));
    entity.setScheduleData(row.at("schedule_data"));
    entity.setSynced(row.at("is_synced") == "1");
    
    if (row.count("last_sync_time") && !row.at("last_sync_time").empty() && row.at("last_sync_time") != "NULL") {
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
    : IRepository<DeviceScheduleEntity>("DeviceScheduleRepository") {
}

std::vector<DeviceScheduleEntity> DeviceScheduleRepository::findAll() {
    DbLib::DatabaseAbstractionLayer db;
    std::string sql = "SELECT * FROM device_schedules";
    auto results = db.executeQuery(sql);
    
    std::vector<DeviceScheduleEntity> entities;
    for (const auto& row : results) {
        entities.push_back(mapRowToEntity(row));
    }
    return entities;
}

std::optional<DeviceScheduleEntity> DeviceScheduleRepository::findById(int id) {
    DbLib::DatabaseAbstractionLayer db;
    std::string sql = "SELECT * FROM device_schedules WHERE id = " + std::to_string(id);
    auto results = db.executeQuery(sql);
    if (results.empty()) return std::nullopt;
    return mapRowToEntity(results[0]);
}

bool DeviceScheduleRepository::save(DeviceScheduleEntity& entity) {
    DbLib::DatabaseAbstractionLayer db;
    std::string sql = "INSERT INTO device_schedules (device_id, point_id, schedule_type, schedule_data, is_synced) VALUES ("
        + std::to_string(entity.getDeviceId()) + ", "
        + (entity.getPointId().has_value() ? std::to_string(entity.getPointId().value()) : "NULL") + ", '"
        + entity.getScheduleType() + "', '"
        + entity.getScheduleData() + "', "
        + (entity.isSynced() ? "1" : "0") + ")";
    
    if (db.executeNonQuery(sql)) {
        // Simple way to get the last inserted ID in SQLite
        auto res = db.executeQuery("SELECT last_insert_rowid() as id");
        if (!res.empty()) {
            entity.setId(std::stoi(res[0].at("id")));
            entity.markSaved();
            return true;
        }
    }
    return false;
}

bool DeviceScheduleRepository::update(const DeviceScheduleEntity& entity) {
    DbLib::DatabaseAbstractionLayer db;
    std::string sql = "UPDATE device_schedules SET "
        "device_id = " + std::to_string(entity.getDeviceId()) + ", "
        "point_id = " + (entity.getPointId().has_value() ? std::to_string(entity.getPointId().value()) : "NULL") + ", "
        "schedule_type = '" + entity.getScheduleType() + "', "
        "schedule_data = '" + entity.getScheduleData() + "', "
        "is_synced = " + (entity.isSynced() ? "1" : "0") + ", "
        "updated_at = (datetime('now', 'localtime')) "
        "WHERE id = " + std::to_string(entity.getId());
    return db.executeNonQuery(sql);
}

bool DeviceScheduleRepository::deleteById(int id) {
    DbLib::DatabaseAbstractionLayer db;
    std::string sql = "DELETE FROM device_schedules WHERE id = " + std::to_string(id);
    return db.executeNonQuery(sql);
}

bool DeviceScheduleRepository::exists(int id) {
    return findById(id).has_value();
}

std::vector<DeviceScheduleEntity> DeviceScheduleRepository::findByDeviceId(int device_id) {
    DbLib::DatabaseAbstractionLayer db;
    std::string sql = "SELECT * FROM device_schedules WHERE device_id = " + std::to_string(device_id);
    auto results = db.executeQuery(sql);
    std::vector<DeviceScheduleEntity> entities;
    for (const auto& row : results) {
        entities.push_back(mapRowToEntity(row));
    }
    return entities;
}

std::vector<DeviceScheduleEntity> DeviceScheduleRepository::findPendingSync(int device_id) {
    DbLib::DatabaseAbstractionLayer db;
    std::string sql = "SELECT * FROM device_schedules WHERE is_synced = 0";
    if (device_id != -1) {
        sql += " AND device_id = " + std::to_string(device_id);
    }
    auto results = db.executeQuery(sql);
    std::vector<DeviceScheduleEntity> entities;
    for (const auto& row : results) {
        entities.push_back(mapRowToEntity(row));
    }
    return entities;
}

bool DeviceScheduleRepository::markAsSynced(int id) {
    DbLib::DatabaseAbstractionLayer db;
    std::string sql = "UPDATE device_schedules SET is_synced = 1, last_sync_time = (datetime('now', 'localtime')) WHERE id = " + std::to_string(id);
    return db.executeNonQuery(sql);
}

} // namespace Repositories
} // namespace Database
} // namespace PulseOne
