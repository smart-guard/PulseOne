#ifndef DEVICE_SCHEDULE_ENTITY_H
#define DEVICE_SCHEDULE_ENTITY_H

/**
 * @file DeviceScheduleEntity.h
 * @brief PulseOne DeviceScheduleEntity - 디바이스용 스케줄 엔티티 (BACnet
 * Weekly_Schedule 등)
 */

#include "Database/Entities/BaseEntity.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace PulseOne {
namespace Database {
namespace Repositories {
class DeviceScheduleRepository;
}
namespace Entities {

using json = nlohmann::json;

class DeviceScheduleEntity : public BaseEntity<DeviceScheduleEntity> {
public:
  DeviceScheduleEntity();
  explicit DeviceScheduleEntity(int id);
  virtual ~DeviceScheduleEntity() = default;

  std::shared_ptr<Repositories::DeviceScheduleRepository> getRepository() const;

  // BaseEntity 순수 가상 함수 구현
  bool loadFromDatabase() override;
  bool saveToDatabase() override;
  bool updateToDatabase() override;
  bool deleteFromDatabase() override;

  json toJson() const override {
    json j;
    j["id"] = getId();
    j["device_id"] = device_id_;
    j["point_id"] = point_id_.has_value() ? point_id_.value() : -1;
    j["schedule_type"] = schedule_type_;
    j["schedule_data"] = schedule_data_;
    j["is_synced"] = is_synced_;
    j["last_sync_time"] = last_sync_time_.has_value()
                              ? timestampToString(last_sync_time_.value())
                              : "";
    return j;
  }

  bool fromJson(const json &data) override {
    try {
      if (data.contains("id"))
        setId(data["id"].get<int>());
      if (data.contains("device_id"))
        setDeviceId(data["device_id"].get<int>());
      if (data.contains("point_id")) {
        int pid = data["point_id"].get<int>();
        if (pid != -1)
          point_id_ = pid;
        else
          point_id_ = std::nullopt;
      }
      if (data.contains("schedule_type"))
        setScheduleType(data["schedule_type"].get<std::string>());
      if (data.contains("schedule_data"))
        setScheduleData(data["schedule_data"].get<std::string>());
      if (data.contains("is_synced"))
        setSynced(data["is_synced"].get<bool>());
      return true;
    } catch (const std::exception &) {
      return false;
    }
  }

  std::string toString() const override {
    return "DeviceScheduleEntity[id=" + std::to_string(getId()) +
           ", device=" + std::to_string(device_id_) +
           ", type=" + schedule_type_ +
           ", synced=" + (is_synced_ ? "yes" : "no") + "]";
  }

  std::string getTableName() const override { return "device_schedules"; }
  virtual std::string getEntityTypeName() const override {
    return "DeviceScheduleEntity";
  }

  // Getters
  int getDeviceId() const { return device_id_; }
  std::optional<int> getPointId() const { return point_id_; }
  const std::string &getScheduleType() const { return schedule_type_; }
  const std::string &getScheduleData() const { return schedule_data_; }
  bool isSynced() const { return is_synced_; }
  std::optional<std::chrono::system_clock::time_point> getLastSyncTime() const {
    return last_sync_time_;
  }

  // Setters
  void setDeviceId(int device_id) {
    device_id_ = device_id;
    markModified();
  }
  void setPointId(std::optional<int> point_id) {
    point_id_ = point_id;
    markModified();
  }
  void setScheduleType(const std::string &type) {
    schedule_type_ = type;
    markModified();
  }
  void setScheduleData(const std::string &data) {
    schedule_data_ = data;
    markModified();
  }
  void setSynced(bool synced) {
    is_synced_ = synced;
    markModified();
  }
  void setLastSyncTime(const std::chrono::system_clock::time_point &time) {
    last_sync_time_ = time;
    markModified();
  }

private:
  int device_id_ = 0;
  std::optional<int> point_id_;
  std::string schedule_type_;
  std::string schedule_data_;
  bool is_synced_ = false;
  std::optional<std::chrono::system_clock::time_point> last_sync_time_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // DEVICE_SCHEDULE_ENTITY_H
