#ifndef PULSEONE_DRIVERS_OPCUA_DRIVER_H
#define PULSEONE_DRIVERS_OPCUA_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include <memory>
#include <string>

namespace PulseOne {
namespace Drivers {

/**
 * @brief OPC-UA 프로토콜 드라이버
 * 
 * 산업용 표준 OPC-UA 서버와 통신을 담당합니다.
 */
class OPCUADriver : public IProtocolDriver {
public:
    OPCUADriver();
    virtual ~OPCUADriver();

    // IProtocolDriver 인터페이스 구현
    bool Initialize(const PulseOne::Structs::DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    // 추가 필수 메서드
    bool Start() override;
    bool Stop() override;
    PulseOne::Structs::DriverStatus GetStatus() const override;
    PulseOne::Structs::ErrorInfo GetLastError() const override;

    bool ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points, 
                   std::vector<PulseOne::Structs::TimestampedValue>& values) override;
    
    bool WriteValue(const PulseOne::Structs::DataPoint& point, 
                   const PulseOne::Structs::DataValue& value) override;

    PulseOne::Enums::ProtocolType GetProtocolType() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace Drivers
} // namespace PulseOne

#endif // PULSEONE_DRIVERS_OPCUA_DRIVER_H
