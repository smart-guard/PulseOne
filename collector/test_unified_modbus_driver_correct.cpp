#include "Drivers/UnifiedModbusDriver.h"
#include <iostream>
#include <vector>

using namespace PulseOne::Drivers;
using namespace PulseOne::New;

int main() {
    std::cout << "🧪 UnifiedModbusDriver 올바른 테스트\n";
    std::cout << "================================\n";
    
    try {
        // 1. 기본 생성자를 사용한 드라이버 생성
        std::cout << "1️⃣ 드라이버 생성 중 (기본 생성자)...\n";
        UnifiedModbusDriver driver;
        std::cout << "✅ 드라이버 생성 성공\n";
        
        // 2. 기존 호환성 메서드를 사용한 설정
        std::cout << "\n2️⃣ 레거시 메서드로 디바이스 설정 중...\n";
        driver.SetLegacyDeviceInfo("test_device_1", "127.0.0.1", 502, 1, true);
        std::cout << "✅ 레거시 디바이스 설정 완료\n";
        
        // 3. 레거시 데이터 포인트 추가
        std::cout << "\n3️⃣ 레거시 데이터 포인트 추가 중...\n";
        driver.AddLegacyDataPoint("holding_reg_1", "Test Holding Register 1", 40001, 3, "uint16");
        driver.AddLegacyDataPoint("holding_reg_2", "Test Holding Register 2", 40002, 3, "uint16");
        driver.AddLegacyDataPoint("coil_1", "Test Coil 1", 1, 1, "bool");
        std::cout << "✅ 레거시 데이터 포인트 3개 추가 완료\n";
        
        // 4. 드라이버 초기화 및 시작 (기존 메서드 사용)
        std::cout << "\n4️⃣ 드라이버 초기화 및 시작 중...\n";
        bool init_result = driver.Initialize();
        std::cout << "✅ 초기화: " << (init_result ? "성공" : "실패") << "\n";
        
        bool start_result = driver.Start();
        std::cout << "✅ 시작: " << (start_result ? "성공" : "실패") << "\n";
        
        // 5. 연결 상태 확인
        std::cout << "\n5️⃣ 연결 상태 확인...\n";
        std::cout << "📊 Modbus 연결 상태: " << (driver.IsModbusConnected() ? "연결됨" : "연결 안됨") << "\n";
        std::cout << "📊 연결 상태 문자열: " << driver.GetConnectionStateString() << "\n";
        
        // 6. 기본 Modbus 기능 테스트 (헤더에 구현된 메서드들)
        std::cout << "\n6️⃣ 기본 Modbus 읽기 테스트...\n";
        
        // Holding Register 읽기
        std::vector<uint16_t> holding_values;
        bool read_result = driver.ReadHoldingRegisters(1, 40001, 3, holding_values);
        std::cout << "✅ Holding Register 읽기: " << (read_result ? "성공" : "실패");
        if (read_result && !holding_values.empty()) {
            std::cout << " (첫 번째 값: " << holding_values[0] << ")";
        }
        std::cout << "\n";
        
        // Input Register 읽기
        std::vector<uint16_t> input_values;
        bool input_result = driver.ReadInputRegisters(1, 30001, 2, input_values);
        std::cout << "✅ Input Register 읽기: " << (input_result ? "성공" : "실패");
        if (input_result && !input_values.empty()) {
            std::cout << " (첫 번째 값: " << input_values[0] << ")";
        }
        std::cout << "\n";
        
        // Coil 읽기
        std::vector<uint8_t> coil_values;
        bool coil_result = driver.ReadCoils(1, 1, 5, coil_values);
        std::cout << "✅ Coil 읽기: " << (coil_result ? "성공" : "실패");
        if (coil_result && !coil_values.empty()) {
            std::cout << " (첫 번째 값: " << (coil_values[0] ? "ON" : "OFF") << ")";
        }
        std::cout << "\n";
        
        // 7. 확장 기능 테스트 (cpp에서 구현한 메서드들)
        std::cout << "\n7️⃣ 확장 기능 테스트...\n";
        
        // 다중 레지스터 쓰기
        std::vector<uint16_t> write_values = {100, 200, 300};
        bool write_result = driver.WriteHoldingRegisters(1, 40001, 3, write_values);
        std::cout << "✅ 다중 레지스터 쓰기: " << (write_result ? "성공" : "실패") << "\n";
        
        // 단일 코일 쓰기
        bool coil_write_result = driver.WriteCoil(1, 1, true);
        std::cout << "✅ 단일 코일 쓰기: " << (coil_write_result ? "성공" : "실패") << "\n";
        
        // 다중 코일 쓰기
        std::vector<uint8_t> coil_write_values = {1, 0, 1, 0, 1};
        bool coils_write_result = driver.WriteCoils(1, 1, 5, coil_write_values);
        std::cout << "✅ 다중 코일 쓰기: " << (coils_write_result ? "성공" : "실패") << "\n";
        
        // Discrete Input 읽기
        std::vector<uint8_t> discrete_values;
        bool discrete_result = driver.ReadDiscreteInputs(1, 10001, 3, discrete_values);
        std::cout << "✅ Discrete Input 읽기: " << (discrete_result ? "성공" : "실패") << "\n";
        
        // 8. 연결 테스트
        std::cout << "\n8️⃣ 연결 테스트...\n";
        bool connection_test = driver.TestConnection();
        std::cout << "✅ 연결 테스트: " << (connection_test ? "성공" : "실패") << "\n";
        
        // 9. 통계 정보 출력
        std::cout << "\n9️⃣ 통계 정보...\n";
        std::string json_stats = driver.GetModbusStatisticsJSON();
        std::cout << "📊 JSON 통계:\n" << json_stats << "\n";
        
        // 10. 상세 진단 정보
        std::cout << "\n🔟 상세 진단 정보...\n";
        std::string diagnostics = driver.GetDetailedDiagnostics();
        std::cout << diagnostics << "\n";
        
        // 11. 통계 초기화 테스트
        std::cout << "\n1️⃣1️⃣ 통계 초기화...\n";
        driver.ResetModbusStatistics();
        std::cout << "✅ 통계 초기화 완료\n";
        
        // 통계 초기화 확인
        std::string reset_stats = driver.GetModbusStatisticsJSON();
        std::cout << "📊 초기화 후 통계:\n" << reset_stats << "\n";
        
        // 12. 정리
        std::cout << "\n1️⃣2️⃣ 드라이버 정리...\n";
        driver.Stop();
        std::cout << "✅ 드라이버 정지 완료\n";
        
        std::cout << "\n🎊 모든 테스트 완료! (올바른 버전)\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 예외 발생: " << e.what() << "\n";
        return 1;
    }
}
