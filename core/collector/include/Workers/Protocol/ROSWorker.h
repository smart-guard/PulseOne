#ifndef PULSEONE_WORKERS_PROTOCOL_ROS_WORKER_H
#define PULSEONE_WORKERS_PROTOCOL_ROS_WORKER_H

#include "Workers/Base/BaseDeviceWorker.h"
#include "Drivers/Ros/ROSDriver.h"
#include <memory>
#include <string>
#include <vector>

namespace PulseOne {
namespace Workers {

class ROSWorker : public BaseDeviceWorker {
public:
    explicit ROSWorker(const PulseOne::Structs::DeviceInfo& device_info);
    
    virtual ~ROSWorker() = default;

    // BaseDeviceWorker Implementation
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    bool EstablishConnection() override;
    bool CloseConnection() override;
    bool CheckConnection() override;

private:
    void HandleRosMessage(const std::string& topic, const nlohmann::json& msg);
    
    // Helper to map ROS JSON to DataPoints
    void DetermineDataPointsFromTopics();

    std::unique_ptr<PulseOne::Drivers::ROSDriver> driver_;
    std::vector<std::string> subscribed_topics_;
    std::unordered_map<std::string, std::vector<PulseOne::Structs::UniqueId>> topic_to_points_;
    std::atomic<bool> is_started_{false};
};

} // namespace Workers
} // namespace PulseOne

#endif // PULSEONE_WORKERS_PROTOCOL_ROS_WORKER_H
