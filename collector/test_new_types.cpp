#include <iostream>
#include <string>

// 기본 구조체 정의 (NewTypes.h 대신 간단한 버전)
namespace PulseOne {
namespace New {

enum class ProtocolType {
    MODBUS_TCP = 1,
    MQTT = 3,
    BACNET_IP = 4
};

struct UnifiedDeviceInfo {
    std::string id;
    std::string name;
    ProtocolType protocol;
    std::string endpoint;
    bool enabled = true;
    
    std::string GetProtocolString() const {
        switch (protocol) {
            case ProtocolType::MODBUS_TCP: return "modbus_tcp";
            case ProtocolType::MQTT: return "mqtt";
            case ProtocolType::BACNET_IP: return "bacnet_ip";
            default: return "unknown";
        }
    }
};

} // namespace New
} // namespace PulseOne

int main() {
    std::cout << "=== PulseOne NewTypes 테스트 ===" << std::endl;
    
    using namespace PulseOne::New;
    
    // Modbus 디바이스 생성
    UnifiedDeviceInfo modbus_device;
    modbus_device.id = "modbus_001";
    modbus_device.name = "PLC 제어기";
    modbus_device.protocol = ProtocolType::MODBUS_TCP;
    modbus_device.endpoint = "192.168.1.100:502";
    
    std::cout << "✅ 디바이스 생성: " << modbus_device.name << std::endl;
    std::cout << "✅ 프로토콜: " << modbus_device.GetProtocolString() << std::endl;
    std::cout << "✅ 엔드포인트: " << modbus_device.endpoint << std::endl;
    
    // MQTT 디바이스 생성
    UnifiedDeviceInfo mqtt_device;
    mqtt_device.id = "mqtt_001";
    mqtt_device.name = "IoT 센서";
    mqtt_device.protocol = ProtocolType::MQTT;
    mqtt_device.endpoint = "mqtt://localhost:1883";
    
    std::cout << "✅ MQTT 디바이스: " << mqtt_device.name << std::endl;
    std::cout << "✅ 프로토콜: " << mqtt_device.GetProtocolString() << std::endl;
    
    std::cout << "\n🎊 UnifiedDeviceInfo 기본 구조 테스트 성공!" << std::endl;
    std::cout << "🚀 이제 실제 NewTypes.h 파일 생성 준비 완료!" << std::endl;
    
    return 0;
}
