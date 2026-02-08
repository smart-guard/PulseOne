#ifndef EDGE_SERVER_ENTITY_H
#define EDGE_SERVER_ENTITY_H

#include "Database/Entities/BaseEntity.h"
#include <chrono>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace PulseOne {
namespace Database {
namespace Entities {

class EdgeServerEntity : public BaseEntity<EdgeServerEntity> {
public:
  EdgeServerEntity();
  explicit EdgeServerEntity(int id);
  virtual ~EdgeServerEntity() = default;

  bool loadFromDatabase() override;
  bool saveToDatabase() override;
  bool deleteFromDatabase() override;
  bool updateToDatabase() override;

  json toJson() const override;
  bool fromJson(const json &data) override;
  std::string toString() const override;
  std::string getTableName() const override { return "edge_servers"; }

  // Repository access
  std::shared_ptr<Repositories::EdgeServerRepository> getRepository() const;

  // Getters and Setters
  int getTenantId() const { return tenant_id_; }
  void setTenantId(int tenant_id) {
    tenant_id_ = tenant_id;
    markModified();
  }

  const std::string &getName() const { return name_; }
  void setName(const std::string &name) {
    name_ = name;
    markModified();
  }

  const std::string &getDescription() const { return description_; }
  void setDescription(const std::string &description) {
    description_ = description;
    markModified();
  }

  const std::string &getIpAddress() const { return ip_address_; }
  void setIpAddress(const std::string &ip_address) {
    ip_address_ = ip_address;
    markModified();
  }

  int getPort() const { return port_; }
  void setPort(int port) {
    port_ = port;
    markModified();
  }

  bool isEnabled() const { return is_enabled_; }
  void setEnabled(bool enabled) {
    is_enabled_ = enabled;
    markModified();
  }

  int getMaxDevices() const { return max_devices_; }
  void setMaxDevices(int count) {
    max_devices_ = count;
    markModified();
  }

  int getMaxDataPoints() const { return max_data_points_; }
  max_data_points_ = count;
  markModified();
}

const std::string &
getSubscriptionMode() const {
  return subscription_mode_;
}
void setSubscriptionMode(const std::string &mode) {
  subscription_mode_ = mode;
  markModified();
}

std::chrono::system_clock::time_point getCreatedAt() const {
  return created_at_;
}
std::chrono::system_clock::time_point getUpdatedAt() const {
  return updated_at_;
}

const json &getConfig() const { return config_; }
void setConfig(const json &config) {
  config_ = config;
  markModified();
}

private:
int tenant_id_;
std::string name_;
std::string description_;
std::string ip_address_;
int port_;
bool is_enabled_;
int max_devices_;
int max_data_points_;
std::string subscription_mode_;
json config_;
std::chrono::system_clock::time_point created_at_;
std::chrono::system_clock::time_point updated_at_;
};

} // namespace Entities
} // namespace Database
} // namespace PulseOne

#endif // EDGE_SERVER_ENTITY_H
