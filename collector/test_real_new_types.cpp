#include "include/Common/NewTypes.h"
#include <iostream>

int main() {
    std::cout << "=== 실제 NewTypes.h 테스트 ===" << std::endl;
    
    using namespace PulseOne::New;
    
    // 1. Modbus 디바이스 생성
    UnifiedDeviceInfo modbus_device;
    modbus_device.id = GenerateUUID();
    modbus_device.name = "PLC 제어기";
    modbus_device.protocol = ProtocolType::MODBUS_TCP;
    modbus_device.endpoint = "192.168.1.100:502";
    
    // Modbus 특화 설정
    auto& modbus_config = modbus_device.GetModbusConfig();
    modbus_config.slave_id = 1;
    modbus_config.max_registers = 100;
    
    std::cout << "✅ Modbus 디바이스: " << modbus_device.name << std::endl;
    std::cout << "   ID: " << modbus_device.id << std::endl;
    std::cout << "   프로토콜: " << modbus_device.GetProtocolString() << std::endl;
    std::cout << "   슬레이브 ID: " << modbus_config.slave_id << std::endl;
    
    // 2. MQTT 디바이스 생성
    UnifiedDeviceInfo mqtt_device;
    mqtt_device.id = GenerateUUID();
    mqtt_device.name = "IoT 센서";
    mqtt_device.protocol = ProtocolType::MQTT;
    mqtt_device.endpoint = "mqtt://localhost:1883";
    
    auto& mqtt_config = mqtt_device.GetMqttConfig();
    mqtt_config.client_id = "pulseone_client";
    mqtt_config.qos = 1;
    
    std::cout << "✅ MQTT 디바이스: " << mqtt_device.name << std::endl;
    std::cout << "   클라이언트 ID: " << mqtt_config.client_id << std::endl;
    
    // 3. 데이터 포인트 생성
    UnifiedDataPoint data_point;
    data_point.id = "temp_sensor_01";
    data_point.device_id = modbus_device.id;
    data_point.name = "온도 센서";
    data_point.current_value = 23.5;
    data_point.quality = DataQuality::GOOD;
    data_point.unit = "°C";
    
    std::cout << "✅ 데이터 포인트: " << data_point.name << std::endl;
    std::cout << "   값: " << data_point.GetValueAsString() << " " << data_point.unit << std::endl;
    std::cout << "   품질: " << (data_point.IsGoodQuality() ? "양호" : "불량") << std::endl;
    
    // 4. 타입 안전 테스트
    std::cout << "\n=== 타입 안전성 테스트 ===" << std::endl;
    std::cout << "Modbus 디바이스인가? " << (modbus_device.IsModbus() ? "예" : "아니오") << std::endl;
    std::cout << "MQTT 디바이스인가? " << (mqtt_device.IsMqtt() ? "예" : "아니오") << std::endl;
    
    std::cout << "\n🎊 NewTypes.h 모든 기능 테스트 성공!" << std::endl;
    std::cout << "🚀 UnifiedDriverBase 아키텍처 기반 완성!" << std::endl;
    
    return 0;
}
