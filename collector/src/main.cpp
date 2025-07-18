// =============================================================================
// collector/src/main.cpp
// PulseOne 데이터 수집기 메인 애플리케이션
// 기존 코드 + 새로운 드라이버 시스템 통합
// =============================================================================

#include <iostream>
#include <signal.h>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>

// 기존 PulseOne 헤더들
#include "Config/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"

// 새로운 드라이버 시스템 헤더들 (있을 때만)
#ifdef HAVE_DRIVER_SYSTEM
#include "Drivers/DriverFactory.h"
#include "Drivers/CommonTypes.h"
using namespace PulseOne::Drivers;
#endif

// 프로토콜 라이브러리 헤더들 (설치된 경우에만)
#ifdef HAVE_LIBMODBUS
#include <modbus.h>
#endif

#ifdef HAVE_PAHO_MQTT
#include <MQTTClient.h>
#endif

#ifdef HAVE_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#endif

#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

using namespace PulseOne;

// 전역 변수들
std::atomic<bool> g_running(true);
std::unique_ptr<ConfigManager> g_config_manager;
std::unique_ptr<DatabaseManager> g_db_manager;
std::unique_ptr<LogManager> g_log_manager;

#ifdef HAVE_DRIVER_SYSTEM
std::vector<std::unique_ptr<IProtocolDriver>> g_drivers;
#endif

// 시그널 핸들러
void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    g_running.store(false);
}

// 시스템 초기화
bool InitializeSystem() {
    try {
        std::cout << "=== PulseOne Collector Starting ===" << std::endl;
        
        // 1. 설정 관리자 초기화
        g_config_manager = std::make_unique<ConfigManager>();
        if (!g_config_manager->Initialize()) {
            std::cerr << "Failed to initialize ConfigManager" << std::endl;
            return false;
        }
        std::cout << "✓ Configuration manager initialized" << std::endl;
        
        // 2. 로그 관리자 초기화
        g_log_manager = std::make_unique<LogManager>();
        if (!g_log_manager->Initialize()) {
            std::cerr << "Failed to initialize LogManager" << std::endl;
            return false;
        }
        std::cout << "✓ Log manager initialized" << std::endl;
        
        // 3. 데이터베이스 관리자 초기화
        g_db_manager = std::make_unique<DatabaseManager>();
        if (!g_db_manager->Initialize()) {
            std::cerr << "Failed to initialize DatabaseManager" << std::endl;
            return false;
        }
        std::cout << "✓ Database manager initialized" << std::endl;
        
#ifdef HAVE_DRIVER_SYSTEM
        // 4. 드라이버 팩토리 초기화 (새 시스템이 있을 때만)
        if (!InitializeDriverFactory()) {
            std::cerr << "Failed to initialize driver factory" << std::endl;
            return false;
        }
        std::cout << "✓ Driver factory initialized" << std::endl;
#else
        std::cout << "✓ Using legacy driver system" << std::endl;
#endif
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "System initialization failed: " << e.what() << std::endl;
        return false;
    }
}

// 메인 함수 (기존 구조 유지)
int main(int argc, char* argv[]) {
    try {
        // 시그널 핸들러 등록
        signal(SIGINT, SignalHandler);   // Ctrl+C
        signal(SIGTERM, SignalHandler);  // 종료 신호
        
        std::cout << "PulseOne Data Collector v1.0.0" << std::endl;
        std::cout << "Starting initialization..." << std::endl;
        
        // 1. 시스템 초기화
        if (!InitializeSystem()) {
            std::cerr << "System initialization failed!" << std::endl;
            return -1;
        }
        
        // 2. 드라이버 로드 및 연결 (기존 구조 유지)
        bool drivers_connected = LoadAndConnectDrivers();
        
        if (!drivers_connected) {
            std::cout << "Running in simulation mode (no drivers connected)" << std::endl;
        }
        
        // 3. 초기 상태 출력
        PrintSystemStatistics();
        
        // 4. 메인 데이터 수집 루프 실행
        RunDataCollectionLoop();
        
        // 5. 정상 종료
        ShutdownSystem();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        
        // 비상 종료
        g_running.store(false);
        ShutdownSystem();
        return -1;
    }
}

// =============================================================================
// 기존 코드와의 호환성을 위한 함수들 (필요시 추가)
// =============================================================================

// 기존 main 함수가 있었다면 이름을 변경하고 호출
// int legacy_main() {
//     // 기존 main 코드
//     return 0;
// }