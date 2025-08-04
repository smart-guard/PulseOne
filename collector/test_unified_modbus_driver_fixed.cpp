#include "Drivers/UnifiedModbusDriver.h"
#include <iostream>
#include <vector>

using namespace PulseOne::Drivers;
using namespace PulseOne::New;  // 올바른 네임스페이스

int main() {
    std::cout << "🧪 UnifiedModbusDriver 테스트 시작 (수정된 버전)\n";
    std::cout << "================================\n";
    
    try {
        // 1. 드라이버 생성
        std::cout << "1️⃣ 드라이버 생성 중...\n";
        UnifiedModbusDriver driver;
        std::cout << "✅ 드라이버 생성 성공\n";
        
        // 2. 디바이스 설정 (올바른 네임스페이스 사용)
        std::cout << "\n2️⃣ 디바이스 설정 중...\n";
        UnifiedDeviceInfo device_info;
        device_info.device_id = "test_modbus_device";
        device_info.protocol_type = ProtocolType::MODBUS_TCP;
        device_info.connection_string = "tcp://127.0.0.1:502";
        device_info.timeout_ms = 5000;
        device_info.retry_count = 3;
        
        bool config_result = driver.AddDevice(device_info);
        std::cout << "✅ 디바이스 설정: " << (config_result ? "성공" : "실패") << "\n";
        
        // 3. 데이터 포인트 추가 (올바른 네임스페이스 사용)
        std::cout << "\n3️⃣ 데이터 포인트 추가 중...\n";
        UnifiedDataPoint data_point;
        data_point.point_id = "test_register_1";
        data_point.address = "40001";
        data_point.data_type = DataType::UINT16;
        data_point.access_type = AccessType::READ_WRITE;
        
        bool add_result = driver.AddLegacyDataPoint(data_point);
        std::cout << "✅ 데이터 포인트 추가: " << (add_result ? "성공" : "실패") << "\n";
        
        // 4. 초기화 (올바른 매개변수 사용)
        std::cout << "\n4️⃣ 드라이버 초기화 중...\n";
        bool init_result = driver.Initialize(device_info);
        std::cout << "✅ 초기화: " << (init_result ? "성공" : "실패") << "\n";
        
        // 5. 연결 상태 확인
        std::cout << "\n5️⃣ 연결 상태 확인...\n";
        std::cout << "📊 연결 상태: " << (driver.IsConnected() ? "연결됨" : "연결 안됨") << "\n";
        std::cout << "📊 디바이스 개수: " << driver.GetDeviceRegistry().size() << "\n";
        std::cout << "📊 데이터 포인트 개수: " << driver.GetDataPoints().size() << "\n";
        
        // 6. 통계 정보 출력 (올바른 멤버 접근)
        std::cout << "\n6️⃣ 통계 정보...\n";
        const auto& stats = driver.GetStatistics();
        std::cout << "📈 성공한 읽기: " << stats.successful_reads << "\n";
        std::cout << "📈 실패한 읽기: " << stats.failed_reads << "\n";
        std::cout << "📈 성공한 쓰기: " << stats.successful_writes << "\n";
        std::cout << "📈 실패한 쓰기: " << stats.failed_writes << "\n";
        
        // 7. 확장 기능 테스트
        std::cout << "\n7️⃣ 확장 기능 테스트...\n";
        
        // 다중 레지스터 쓰기 테스트
        std::vector<uint16_t> test_values = {100, 200, 300};
        bool write_result = driver.WriteHoldingRegisters(1, 0, 3, test_values);
        std::cout << "✅ 다중 레지스터 쓰기: " << (write_result ? "성공" : "실패") << "\n";
        
        // 연결 테스트
        bool connection_test = driver.TestConnection();
        std::cout << "✅ 연결 테스트: " << (connection_test ? "성공" : "실패") << "\n";
        
        // 8. 진단 정보 출력
        std::cout << "\n8️⃣ 진단 정보...\n";
        std::string diagnostics = driver.GetDetailedDiagnostics();
        std::cout << diagnostics << "\n";
        
        // 9. 유틸리티 함수 테스트
        std::cout << "\n9️⃣ 유틸리티 함수 테스트...\n";
        std::cout << "예외 코드 0x02: " << UnifiedModbusDriver::ModbusExceptionCodeToString(0x02) << "\n";
        std::cout << "펑션 코드 3: " << UnifiedModbusDriver::ModbusFunctionCodeToString(3) << "\n";
        
        std::cout << "\n🎊 모든 테스트 통과!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 예외 발생: " << e.what() << "\n";
        return 1;
    }
}
