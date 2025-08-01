// =============================================================================
// include/Drivers/Bacnet/BACnetDriver.h
// ModbusDriver 구조를 복사한 최소한의 BACnet Driver
// =============================================================================

#ifndef BACNET_DRIVER_H
#define BACNET_DRIVER_H

#include "Drivers/Common/IProtocolDriver.h"
#include "Common/UnifiedCommonTypes.h"
#include <memory>
#include <atomic>
#include <vector>
#include <map>
#include <mutex>

// 네임스페이스 별칭 (ModbusDriver와 동일)
namespace Utils = PulseOne::Utils;
namespace Constants = PulseOne::Constants;
namespace Structs = PulseOne::Structs;
namespace Enums = PulseOne::Enums;

namespace PulseOne {
namespace Drivers {

/**
 * @brief BACnet 프로토콜 드라이버 (최소한 구현)
 */
class BACnetDriver : public IProtocolDriver {
public:
    BACnetDriver();
    ~BACnetDriver() override;
    
    // IProtocolDriver 인터페이스 (ModbusDriver와 동일한 시그니처)
    bool Initialize(const DriverConfig& config) override;
    bool Connect() override;
    bool Disconnect() override;
    bool IsConnected() const override;
    
    bool ReadValues(const std::vector<Structs::DataPoint>& points,
                   std::vector<TimestampedValue>& values) override;
    
    bool WriteValue(const Structs::DataPoint& point, 
                   const Structs::DataValue& value) override;
    
    Enums::ProtocolType GetProtocolType() const override;
    Structs::DriverStatus GetStatus() const override;
    Structs::ErrorInfo GetLastError() const override;
    
    // 추가 메서드들 (ModbusDriver에는 없지만 IProtocolDriver에 있을 수 있는 것들)
    const DriverStatistics& GetStatistics() const override;
    void ResetStatistics() override;

private:
    // ModbusDriver와 유사한 멤버 변수들
    DriverConfig config_;
    std::atomic<bool> is_connected_{false};
    Structs::ErrorInfo last_error_;
    mutable DriverStatistics statistics_;
    mutable std::mutex stats_mutex_;
    
    // BACnet 관련 포인터 (실제 구현은 나중에)
    void* bacnet_ctx_{nullptr};  // BACnet context (스텁)
    
    // 헬퍼 메서드들
    void SetError(Enums::ErrorCode code, const std::string& message);
    void UpdateStatistics(bool success, double response_time_ms);
    Structs::DataValue ConvertBACnetValue(const Structs::DataPoint& point, float raw_value);
};

} // namespace PulseOne::Drivers
} // namespace PulseOne

#endif // BACNET_DRIVER_H