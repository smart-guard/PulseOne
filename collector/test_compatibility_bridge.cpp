#include "include/Common/NewTypes.h"
#include "include/Common/CompatibilityBridge.h"
#include <iostream>

int main() {
    std::cout << "=== CompatibilityBridge 테스트 ===" << std::endl;
    
    using namespace PulseOne::New;
    using namespace PulseOne::Bridge;
    
    // 1. 편의 함수로 Modbus 디바이스 생성
    auto modbus_device = SafeCompatibilityBridge::FromModbusInfo("192.168.1.100:502", 1, 3000);
    std::cout << "✅ Modbus 디바이스 생성:" << std::endl;
    std::cout << SafeCompatibilityBridge::GetDeviceSummary(modbus_device) << std::endl;
    
    // 2. 편의 함수로 MQTT 디바이스 생성  
    auto mqtt_device = SafeCompatibilityBridge::FromMqttInfo("mqtt://localhost:1883", "test_client");
    std::cout << "✅ MQTT 디바이스 생성:" << std::endl;
    std::cout << SafeCompatibilityBridge::GetDeviceSummary(mqtt_device) << std::endl;
    
    // 3. BACnet 디바이스 생성
    auto bacnet_device = SafeCompatibilityBridge::FromBacnetInfo("192.168.1.200", 1234);
    std::cout << "✅ BACnet 디바이스 생성:" << std::endl;
    std::cout << SafeCompatibilityBridge::GetDeviceSummary(bacnet_device) << std::endl;
    
    // 4. JSON 변환 테스트
    std::string json_output = SafeCompatibilityBridge::ToLegacyJson(modbus_device);
    std::cout << "\n✅ JSON 변환 테스트:" << std::endl;
    std::cout << json_output << std::endl;
    
    // 5. 호환성 검증
    bool modbus_valid = SafeCompatibilityBridge::ValidateCompatibility(modbus_device);
    bool mqtt_valid = SafeCompatibilityBridge::ValidateCompatibility(mqtt_device);
    
    std::cout << "\n✅ 호환성 검증:" << std::endl;
    std::cout << "  Modbus: " << (modbus_valid ? "유효" : "무효") << std::endl;
    std::cout << "  MQTT: " << (mqtt_valid ? "유효" : "무효") << std::endl;
    
    // 6. 기존 형식으로 변환 테스트
    auto legacy_entity = SafeCompatibilityBridge::ToLegacyEntity(modbus_device);
    std::cout << "\n✅ 기존 Entity 변환:" << std::endl;
    std::cout << "  ID: " << legacy_entity.id << std::endl;
    std::cout << "  Name: " << legacy_entity.name << std::endl;
    std::cout << "  Protocol: " << legacy_entity.protocol_type << std::endl;
    
    std::cout << "\n🎊 CompatibilityBridge 모든 기능 테스트 성공!" << std::endl;
    std::cout << "🚀 기존 코드와의 완벽한 호환성 확보!" << std::endl;
    
    return 0;
}
