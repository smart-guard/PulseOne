/**
 * @file HttpRestWorker.h
 * @brief HTTP/REST Protocol Worker
 * @author PulseOne Development Team
 * @date 2026-01-07
 */

#ifndef HTTP_REST_WORKER_H
#define HTTP_REST_WORKER_H

#include <string>
#include <memory>
#include <vector>
#include <future>

#include "Workers/Base/BaseDeviceWorker.h"
#include "Common/Structs.h"
#include "Common/Enums.h"
#include "Drivers/Common/IProtocolDriver.h"

namespace PulseOne {
namespace Workers {

using DeviceInfo = PulseOne::Structs::DeviceInfo;
using DataPoint = PulseOne::Structs::DataPoint;
using DataValue = PulseOne::Structs::DataValue;

/**
 * @brief HTTP/REST Protocol Worker
 */
class HttpRestWorker : public BaseDeviceWorker {
public:
    explicit HttpRestWorker(const DeviceInfo& device_info);
    virtual ~HttpRestWorker();

    // BaseDeviceWorker Interface Implementation
    std::future<bool> Start() override;
    std::future<bool> Stop() override;
    bool EstablishConnection() override;
    bool CloseConnection() override;
    bool CheckConnection() override;
    bool SendKeepAlive() override;

    // Write Interface
    bool WriteDataPoint(const std::string& point_id, const DataValue& value) override;

protected:
    // Internal methods
    bool ParseHttpRestConfig();
    bool InitializeHttpRestDriver();
    
    void PollingThreadFunction();

private:
    std::unique_ptr<PulseOne::Drivers::IProtocolDriver> http_driver_;
    PulseOne::Structs::DriverConfig http_config_;
    
    std::atomic<bool> polling_thread_running_;
    std::unique_ptr<std::thread> polling_thread_;
};

} // namespace Workers
} // namespace PulseOne

#endif // HTTP_REST_WORKER_H
