// ============================================================================
// Path Corrected MQTT Driver Test
// ============================================================================

// 실제 파일 구조에 맞는 include 경로 사용
#include "../include/Drivers/Mqtt/MQTTDriver.h"
#include <iostream>
#include <memory>

using namespace PulseOne::Drivers;

#define SIMPLE_TEST(condition, message) \
    if (condition) { \
        std::cout << "✅ " << message << std::endl; \
    } else { \
        std::cout << "❌ " << message << std::endl; \
        test_failed = true; \
    }

int main() {
    std::cout << "🧪 MQTT Driver Test (Corrected Paths)" << std::endl;
    std::cout << "====================================" << std::endl;
    
    bool test_failed = false;
    
    try {
        // 테스트 1: 드라이버 생성
        std::cout << "\n🔧 Test 1: Driver Creation" << std::endl;
        auto driver = std::make_unique<MqttDriver>();
        SIMPLE_TEST(driver != nullptr, "Driver created successfully");
        
        // 테스트 2: 프로토콜 타입 확인
        std::cout << "\n🔧 Test 2: Protocol Type" << std::endl;
        ProtocolType type = driver->GetProtocolType();
        SIMPLE_TEST(type == ProtocolType::MQTT, "Protocol type is MQTT");
        
        // 테스트 3: MqttDataPointInfo 구조체 테스트
        std::cout << "\n🔧 Test 3: MqttDataPointInfo Structure" << std::endl;
        
        // 기본 생성자
        MqttDriver::MqttDataPointInfo default_point;
        SIMPLE_TEST(default_point.qos == 1, "Default QoS is 1");
        SIMPLE_TEST(!default_point.is_writable, "Default is not writable");
        SIMPLE_TEST(default_point.auto_subscribe, "Default auto subscribe is true");
        
        // 매개변수 생성자
        MqttDriver::MqttDataPointInfo param_point(
            "test_point_01",
            "Test Point",
            "Test description", 
            "test/topic",
            2,
            DataType::FLOAT32
        );
        SIMPLE_TEST(param_point.point_id == "test_point_01", "Point ID set correctly");
        SIMPLE_TEST(param_point.name == "Test Point", "Point name set correctly");
        SIMPLE_TEST(param_point.qos == 2, "QoS set correctly");
        SIMPLE_TEST(param_point.data_type == DataType::FLOAT32, "Data type set correctly");
        
        std::cout << "\n🎉 Structure tests completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception: " << e.what() << std::endl;
        test_failed = true;
    }
    
    if (!test_failed) {
        std::cout << "✅ SUCCESS: Basic MQTT structure tests passed!" << std::endl;
        return 0;
    } else {
        std::cout << "❌ FAILED: Some tests failed" << std::endl;
        return 1;
    }
}
