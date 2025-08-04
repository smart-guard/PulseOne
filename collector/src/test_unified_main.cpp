// =============================================================================
// Phase 1-D: 컴파일 테스트용 메인 파일
// =============================================================================

// collector/src/test_unified_main.cpp (별도 테스트 실행 파일)
/*
#include "Drivers/TestUnifiedDriverBase.h"
#include <iostream>

int main() {
    using namespace PulseOne::Drivers::Test;
    using namespace PulseOne::New;
    using namespace PulseOne::Bridge;
    
    std::cout << "=== PulseOne 통합 아키텍처 안전 테스트 ===" << std::endl;
    
    try {
        // 1. 통합 디바이스 정보 생성 테스트
        auto modbus_device = SafeCompatibilityBridge::FromModbusInfo("192.168.1.100:502", 1, 3000);
        std::cout << "✅ Modbus 디바이스 생성: " << modbus_device.name << std::endl;
        
        auto mqtt_device = SafeCompatibilityBridge::FromMqttInfo("mqtt://localhost:1883", "test_client");
        std::cout << "✅ MQTT 디바이스 생성: " << mqtt_device.name << std::endl;
        
        // 2. 테스트 드라이버 생성 및 실행
        auto driver = std::make_unique<TestModbusDriver>();
        
        // 3. 데이터 콜백 설정
        driver->SetDataCallback([](const UnifiedDeviceInfo& device, 
                                  const std::vector<UnifiedDataPoint>& points) {
            std::cout << "📊 데이터 수신: " << device.name << " (" << points.size() << " points)" << std::endl;
        });
        
        // 4. 드라이버 초기화 및 시작
        if (driver->Initialize(modbus_device)) {
            std::cout << "✅ 드라이버 초기화 성공" << std::endl;
            
            if (driver->Start()) {
                std::cout << "✅ 드라이버 시작 성공" << std::endl;
                
                // 5초간 실행 후 정지
                std::this_thread::sleep_for(std::chrono::seconds(5));
                
                driver->Stop();
                std::cout << "✅ 드라이버 정지 완료" << std::endl;
            }
        }
        
        std::cout << "🎊 모든 테스트 완료 - 컴파일 및 실행 성공!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 테스트 실패: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
*/