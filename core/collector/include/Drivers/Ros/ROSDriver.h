#ifndef PULSEONE_DRIVERS_ROS_DRIVER_H
#define PULSEONE_DRIVERS_ROS_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

namespace PulseOne {
namespace Drivers {

/**
 * @brief ROS Bridge (TCP) Driver
 * 
 * Connects to a `rosbridge_server` via TCP (default port 9090).
 * Protocols:
 *  - Subscribe: {"op": "subscribe", "topic": "/..."}
 *  - Publish: {"op": "publish", "topic": "/...", "msg": {...}}
 */
class ROSDriver : public IProtocolDriver {
public:
    ROSDriver();
    virtual ~ROSDriver();

    // IProtocolDriver Implementation
    bool Initialize(const PulseOne::Structs::DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool Start() override;
    bool Stop() override;
    
    PulseOne::Structs::DriverStatus GetStatus() const override;
    PulseOne::Structs::ErrorInfo GetLastError() const override;

    // Not used in asynchronous push model, but implemented for interface compliance
    bool ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points, 
                   std::vector<PulseOne::Structs::TimestampedValue>& values) override;
    
    bool WriteValue(const PulseOne::Structs::DataPoint& point, 
                   const PulseOne::Structs::DataValue& value) override;

    PulseOne::Enums::ProtocolType GetProtocolType() const override;

    // ROS Specific Methods
    bool Subscribe(const std::string& topic);
    
    // Callback for incoming messages (Worker registers this)
    using MessageCallback = std::function<void(const std::string& topic, const nlohmann::json& msg)>;
    void SetMessageCallback(MessageCallback callback);

private:
    void ReceiveLoop();
    void UpdateStatus(PulseOne::Structs::DriverStatus status);
    void SetError(const std::string& message, PulseOne::Structs::ErrorCode code = PulseOne::Structs::ErrorCode::DEVICE_ERROR);

    // Socket helpers
    bool OpenSocket();
    void CloseSocket();
    bool SendJson(const nlohmann::json& j);

    // Members
    PulseOne::Structs::DriverConfig config_;
    std::string robot_ip_;
    int robot_port_ = 9090;

    int socket_fd_ = -1;
    std::atomic<bool> is_running_{false};
    std::atomic<bool> is_connected_{false};
    PulseOne::Structs::DriverStatus status_ = PulseOne::Structs::DriverStatus::UNINITIALIZED;

    std::unique_ptr<std::thread> receive_thread_;
    std::mutex socket_mutex_;
    std::mutex callback_mutex_;
    
    MessageCallback message_callback_;
    
    // Buffer for partial reads
    std::string receive_buffer_;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_ROS_DRIVER_H
