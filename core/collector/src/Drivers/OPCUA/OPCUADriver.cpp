#include "Drivers/OPCUA/OPCUADriver.h"
#include "Drivers/Common/DriverFactory.h"
#include "Utils/LogManager.h"
#include <chrono>
#include <thread>

namespace PulseOne {
namespace Drivers {

struct OPCUADriver::Impl {
    PulseOne::Structs::DriverConfig config;
    bool is_connected = false;
};

OPCUADriver::OPCUADriver() : impl_(std::make_unique<Impl>()) {}
OPCUADriver::~OPCUADriver() { Disconnect(); }

bool OPCUADriver::Initialize(const PulseOne::Structs::DriverConfig& config) {
    impl_->config = config;
    LogManager::getInstance().Info("OPC-UA Driver initialized for: " + config.endpoint);
    return true;
}

bool OPCUADriver::Connect() {
    LogManager::getInstance().Info("Connecting to OPC-UA server: " + impl_->config.endpoint);
    // TODO: 실제 open62541 또는 다른 스택 연동
    impl_->is_connected = true;
    return true;
}

bool OPCUADriver::Disconnect() {
    impl_->is_connected = false;
    return true;
}

bool OPCUADriver::IsConnected() const {
    return impl_->is_connected;
}

bool OPCUADriver::ReadValues(const std::vector<PulseOne::Structs::DataPoint>& points, 
                            std::vector<PulseOne::Structs::TimestampedValue>& values) {
    if (!impl_->is_connected) return false;

    for (const auto& point : points) {
        PulseOne::Structs::TimestampedValue tv;
        tv.point_id = std::stoi(point.id);
        tv.timestamp = std::chrono::system_clock::now();
        tv.quality = PulseOne::Enums::DataQuality::GOOD;
        tv.value = 0.0; // Mock value
        values.push_back(tv);
    }
    return true;
}

bool OPCUADriver::WriteValue(const PulseOne::Structs::DataPoint& point, 
                           const PulseOne::Structs::DataValue& value) {
    if (!impl_->is_connected) return false;
    LogManager::getInstance().Info("OPC-UA Write: Node=" + point.name);
    return true;
}

bool OPCUADriver::Start() {
    return Connect();
}

bool OPCUADriver::Stop() {
    return Disconnect();
}

PulseOne::Structs::DriverStatus OPCUADriver::GetStatus() const {
    if (impl_->is_connected) {
        return PulseOne::Structs::DriverStatus::RUNNING;
    }
    return PulseOne::Structs::DriverStatus::STOPPED;
}

PulseOne::Structs::ErrorInfo OPCUADriver::GetLastError() const {
    return last_error_; // IProtocolDriver의 protected 멤버 리턴
}

PulseOne::Enums::ProtocolType OPCUADriver::GetProtocolType() const {
    return PulseOne::Enums::ProtocolType::OPC_UA;
}

// =============================================================================
// 🔥 플러그인 등록용 C 인터페이스 (PluginLoader가 호출)
// =============================================================================
extern "C" {
#ifdef _WIN32
    __declspec(dllexport) void RegisterPlugin() {
#else
    void RegisterPlugin() {
#endif
        DriverFactory::GetInstance().RegisterDriver("OPC_UA", []() {
            return std::make_unique<OPCUADriver>();
        });
    }
}

} // namespace Drivers
} // namespace PulseOne

