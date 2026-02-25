#ifndef PULSEONE_WORKERS_PROTOCOL_ROS_WORKER_H
#define PULSEONE_WORKERS_PROTOCOL_ROS_WORKER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include <atomic>
#include <future>
#include <memory>
#include <nlohmann/json.hpp> // Added for full JSON type definition
#include <string>
#include <unordered_map>
#include <vector>

namespace PulseOne {
namespace Workers {

class ROSWorker : public BaseDeviceWorker {
public:
  explicit ROSWorker(const PulseOne::Structs::DeviceInfo &device_info);

  virtual ~ROSWorker() = default;

  // BaseDeviceWorker Implementation
  std::future<bool> Start() override;
  std::future<bool> Stop() override;
  bool EstablishConnection() override;
  bool CloseConnection() override;
  bool CheckConnection() override;

private:
  // Changed payload type to nlohmann::json to match IProtocolDriver interface
  // type
  void HandleRosMessage(const std::string &topic, const std::string &payload);

  // Helper to map ROS JSON to DataPoints
  void DetermineDataPointsFromTopics();

  std::unique_ptr<PulseOne::Drivers::IProtocolDriver> driver_;
  std::vector<std::string> subscribed_topics_;
  std::unordered_map<std::string, std::vector<PulseOne::Structs::UniqueId>>
      topic_to_points_;
  std::atomic<bool> is_started_{false};
};

} // namespace Workers
} // namespace PulseOne

#endif // PULSEONE_WORKERS_PROTOCOL_ROS_WORKER_H
