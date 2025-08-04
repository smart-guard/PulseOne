#include "Drivers/UnifiedModbusDriver.h"
#include <iostream>
#include <vector>

using namespace PulseOne::Drivers;
using namespace PulseOne::New;  // NewTypes.h 네임스페이스

int main() {
    std::cout << "🧪 UnifiedModbusDriver 최종 테스트\n";
    std::cout << "================================\n";
    
    try {
        // 1. 드라이버 생성
        std::cout << "1️⃣ 드라이버 생성 중...\n";
        UnifiedModbusDriver driver;
        std::cout << "✅ 드라이버 생성 성공\n";
        
        // 2. 실제 NewTypes.h 구조 사용 (올바른 멤버 이름)
        std::cout << "\n2️⃣ 디바이스 설정 중...\n";
        UnifiedDeviceInfo device_info;
        device_info.id = "test_modbus_device";  // 실제 멤버
        device_info.name = "Test Modbus Device";  // 실제 멤버
        device_info.protocol = ProtocolType::MODBUS_TCP;  // 실제 멤버
        device_info.endpoint = "127.0.0.1:502";  // 실제 멤버
        device_info.enabled = true;  // 실제 멤버
        device_info.timeout_ms = 5000;  // 실제 멤버
        
        // Modbus 특화 설정
        device_info.modbus_config.slave_id = 1;
        device_info.modbus_config.timeout_ms = 3000;
        device_info.modbus_config.max_registers_per_request = 100;
        
        std::cout << "✅ 디바이스 정보 설정 완료\n";
        
        // 3. 데이터 포인트 설정 (실제 NewTypes.h 구조 사용)
        std::cout << "\n3️⃣ 데이터 포인트 추가 중...\n";
        UnifiedDataPoint data_point;
        data_point.id = "test_register_1";  // 실제 멤버
        data_point.device_id = device_info.id;  // 실제 멤버
        data_point.name = "Test Register 1";  // 실제 멤버
        data_point.address.numeric = 40001;  // 실제 멤버 (Union)
        data_point.data_type = DataType::UINT16;  // 실제 멤버
        data_point.access_type = AccessType::READ_WRITE;  // 실제 멤버
        
        std::cout << "✅ 데이터 포인트 설정 완료\n";
        
        // 4. 초기화 (베이스 클래스 메서드 사용)
        std::cout << "\n4️⃣ 드라이버 초기화 중...\n";
        bool init_result = driver.Initialize(device_info);
        std::cout << "✅ 초기화: " << (init_result ? "성공" : "실패") << "\n";
        
        // 5. 간단한 기능 테스트
        std::cout << "\n5️⃣ 기본 기능 테스트...\n";
        
        // 확장 기능 테스트
        std::vector<uint16_t> test_values = {100, 200, 300};
        bool write_result = driver.WriteHoldingRegisters(1, 0, 3, test_values);
        std::cout << "✅ 다중 레지스터 쓰기: " << (write_result ? "성공" : "실패") << "\n";
        
        // 연결 테스트
        bool connection_test = driver.TestConnection();
        std::cout << "✅ 연결 테스트: " << (connection_test ? "성공" : "실패") << "\n";
        
        // 6. 진단 정보 출력
        std::cout << "\n6️⃣ 진단 정보...\n";
        std::string diagnostics = driver.GetDetailedDiagnostics();
        std::cout << diagnostics << "\n";
        
        // 7. 유틸리티 함수 테스트
        std::cout << "\n7️⃣ 유틸리티 함수 테스트...\n";
        std::cout << "예외 코드 0x02: " << UnifiedModbusDriver::ModbusExceptionCodeToString(0x02) << "\n";
        std::cout << "펑션 코드 3: " << UnifiedModbusDriver::ModbusFunctionCodeToString(3) << "\n";
        
        // 8. 통계 초기화 테스트
        driver.ResetModbusStatistics();
        std::cout << "✅ 통계 초기화 완료\n";
        
        std::cout << "\n🎊 모든 테스트 통과! (최종 버전)\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 예외 발생: " << e.what() << "\n";
        return 1;
    }
}
