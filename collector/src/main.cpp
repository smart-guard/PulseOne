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

// 새로운 드라이버 시스템 헤더들
#include "Drivers/DriverFactory.h"
#include "Drivers/CommonTypes.h"

using namespace PulseOne;
using namespace PulseOne::Drivers;

// 전역 변수들
std::atomic<bool> g_running(true);
std::vector<std::unique_ptr<IProtocolDriver>> g_drivers;
std::unique_ptr<ConfigManager> g_config_manager;
std::unique_ptr<DatabaseManager> g_db_manager;
std::unique_ptr<LogManager> g_log_manager;

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
        
        // 4. 드라이버 팩토리 초기화
        if (!InitializeDriverFactory()) {
            std::cerr << "Failed to initialize driver factory" << std::endl;
            return false;
        }
        std::cout << "✓ Driver factory initialized" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "System initialization failed: " << e.what() << std::endl;
        return false;
    }
}

// 드라이버 로드 및 연결
bool LoadAndConnectDrivers() {
    try {
        // 환경 변수에서 설정 파일 경로 결정
        const char* env_stage = std::getenv("ENV_STAGE");
        std::string stage = env_stage ? env_stage : "dev";
        
        std::string config_path = "config/drivers/drivers." + stage + ".conf";
        std::cout << "\nLoading driver configuration from: " << config_path << std::endl;
        
        // 설정 파일에서 드라이버들 생성
        try {
            g_drivers = CreateDriversFromConfig(config_path);
            std::cout << "✓ Successfully loaded " << g_drivers.size() << " drivers" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "⚠ Primary config failed, trying fallback: " << e.what() << std::endl;
            
            // 폴백: 기본 설정 파일 시도
            config_path = "config/drivers.conf";
            g_drivers = CreateDriversFromConfig(config_path);
            std::cout << "✓ Successfully loaded " << g_drivers.size() << " drivers from fallback" << std::endl;
        }
        
        if (g_drivers.empty()) {
            std::cout << "⚠ No drivers loaded. Creating default test drivers..." << std::endl;
            CreateDefaultTestDrivers();
        }
        
        // 각 드라이버 연결 시도
        int connected_count = 0;
        std::cout << "\n=== Connecting to devices ===" << std::endl;
        
        for (auto& driver : g_drivers) {
            std::string protocol_name = ProtocolTypeToString(driver->GetProtocolType());
            std::cout << "Connecting to " << protocol_name << " driver..." << std::endl;
            
            if (driver->Connect()) {
                std::cout << "✓ " << protocol_name << " connected successfully" << std::endl;
                connected_count++;
                
                // 로그 기록
                g_log_manager->Info("Driver connected: " + protocol_name);
            } else {
                auto error = driver->GetLastError();
                std::cout << "✗ " << protocol_name << " connection failed: " << error.message << std::endl;
                
                // 에러 로그 기록
                g_log_manager->Error("Driver connection failed: " + protocol_name + " - " + error.message);
            }
        }
        
        std::cout << "\nConnected drivers: " << connected_count << " / " << g_drivers.size() << std::endl;
        
        if (connected_count == 0) {
            std::cout << "⚠ No drivers connected. Running in simulation mode." << std::endl;
            return false;  // 시뮬레이션 모드로 실행
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Driver loading failed: " << e.what() << std::endl;
        return false;
    }
}

// 기본 테스트 드라이버 생성 (설정 파일이 없을 때)
void CreateDefaultTestDrivers() {
    try {
        // Modbus TCP 테스트 드라이버
        auto modbus_driver = CreateDriverSimple("modbus_tcp", "127.0.0.1:502", "test_modbus");
        if (modbus_driver) {
            g_drivers.push_back(std::move(modbus_driver));
            std::cout << "✓ Default Modbus TCP driver created" << std::endl;
        }
        
        // MQTT 테스트 드라이버
        auto mqtt_driver = CreateDriverSimple("mqtt", "mqtt://localhost:1883", "test_mqtt");
        if (mqtt_driver) {
            g_drivers.push_back(std::move(mqtt_driver));
            std::cout << "✓ Default MQTT driver created" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "⚠ Failed to create default drivers: " << e.what() << std::endl;
    }
}

// 테스트용 데이터 포인트 생성
std::vector<DataPoint> CreateTestDataPoints(ProtocolType protocol) {
    std::vector<DataPoint> points;
    
    switch (protocol) {
        case ProtocolType::MODBUS_TCP: {
            // Modbus 홀딩 레지스터 읽기
            DataPoint point1;
            point1.id = GenerateUUID();
            point1.name = "Temperature";
            point1.address = 40001;  // 홀딩 레지스터 1
            point1.data_type = DataType::INT16;
            points.push_back(point1);
            
            DataPoint point2;
            point2.id = GenerateUUID();
            point2.name = "Pressure";
            point2.address = 40002;  // 홀딩 레지스터 2
            point2.data_type = DataType::FLOAT;
            points.push_back(point2);
            break;
        }
        
        case ProtocolType::MQTT: {
            // MQTT 토픽 구독
            DataPoint point1;
            point1.id = GenerateUUID();
            point1.name = "sensor/temperature";
            point1.address = 0;  // MQTT는 주소 대신 토픽 사용
            point1.data_type = DataType::FLOAT;
            points.push_back(point1);
            
            DataPoint point2;
            point2.id = GenerateUUID();
            point2.name = "sensor/humidity";
            point2.address = 0;
            point2.data_type = DataType::FLOAT;
            points.push_back(point2);
            break;
        }
        
        case ProtocolType::BACNET_IP: {
            // BACnet 아날로그 입력 읽기
            DataPoint point1;
            point1.id = GenerateUUID();
            point1.name = "AI_Temperature";
            point1.address = 260001;  // 실제로는 ParseDataPoint에서 파싱
            point1.data_type = DataType::FLOAT;
            points.push_back(point1);
            break;
        }
        
        default:
            break;
    }
    
    return points;
}

// 메인 데이터 수집 루프
void RunDataCollectionLoop() {
    std::cout << "\n=== Starting data collection loop ===" << std::endl;
    
    int cycle_count = 0;
    auto last_stats_time = std::chrono::steady_clock::now();
    
    while (g_running.load()) {
        cycle_count++;
        auto cycle_start = std::chrono::steady_clock::now();
        
        std::cout << "\n--- Collection cycle " << cycle_count << " ---" << std::endl;
        
        // 연결된 각 드라이버에서 데이터 수집
        for (auto& driver : g_drivers) {
            if (!driver->IsConnected()) {
                continue;  // 연결되지 않은 드라이버 건너뛰기
            }
            
            std::string protocol_name = ProtocolTypeToString(driver->GetProtocolType());
            
            try {
                // 테스트용 데이터 포인트 생성
                std::vector<DataPoint> test_points = CreateTestDataPoints(driver->GetProtocolType());
                std::vector<TimestampedValue> values;
                
                // 데이터 읽기
                if (driver->ReadValues(test_points, values)) {
                    std::cout << protocol_name << ": Read " << values.size() << " values successfully" << std::endl;
                    
                    // 데이터베이스에 저장
                    for (size_t i = 0; i < values.size() && i < test_points.size(); ++i) {
                        // 실제 구현에서는 DB 저장 로직 추가
                        // g_db_manager->SaveDataPoint(test_points[i], values[i]);
                    }
                    
                    // 로그 기록
                    g_log_manager->Debug("Data collection successful: " + protocol_name + 
                                       " (" + std::to_string(values.size()) + " values)");
                } else {
                    auto error = driver->GetLastError();
                    std::cout << protocol_name << ": Read failed - " << error.message << std::endl;
                    
                    // 에러 로그 기록
                    g_log_manager->Error("Data collection failed: " + protocol_name + " - " + error.message);
                    
                    // 연결 재시도
                    if (error.code == ErrorCode::CONNECTION_LOST) {
                        std::cout << protocol_name << ": Attempting to reconnect..." << std::endl;
                        driver->Connect();
                    }
                }
                
            } catch (const std::exception& e) {
                std::cout << protocol_name << ": Exception - " << e.what() << std::endl;
                g_log_manager->Error("Data collection exception: " + protocol_name + " - " + e.what());
            }
        }
        
        // 주기적으로 통계 출력 (30초마다)
        auto now = std::chrono::steady_clock::now();
        auto stats_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time);
        
        if (stats_elapsed.count() >= 30) {
            PrintSystemStatistics();
            last_stats_time = now;
        }
        
        // 수집 주기 대기 (5초)
        auto cycle_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - cycle_start);
        
        int sleep_time = 5000 - static_cast<int>(cycle_duration.count());
        if (sleep_time > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        }
    }
    
    std::cout << "\nData collection loop stopped after " << cycle_count << " cycles" << std::endl;
}

// 시스템 통계 출력
void PrintSystemStatistics() {
    std::cout << "\n=== System Statistics ===" << std::endl;
    
    // 드라이버 통계
    for (const auto& driver : g_drivers) {
        if (!driver->IsConnected()) {
            continue;
        }
        
        std::string protocol_name = ProtocolTypeToString(driver->GetProtocolType());
        auto stats = driver->GetStatistics();
        
        std::cout << protocol_name << ": "
                 << "Ops=" << stats.total_operations 
                 << ", Success=" << stats.successful_operations
                 << ", Failed=" << stats.failed_operations;
        
        if (stats.total_operations > 0) {
            auto avg_time = stats.total_response_time / stats.total_operations;
            std::cout << ", AvgTime=" << avg_time.count() << "ms";
        }
        
        std::cout << std::endl;
    }
    
    // 팩토리 통계
    auto& factory_manager = DriverFactoryManager::GetInstance();
    auto factory_stats = factory_manager.GetStatistics();
    
    std::cout << "Factory: "
             << "Protocols=" << factory_stats.total_protocols
             << ", Created=" << factory_stats.successful_creations
             << ", Failed=" << factory_stats.failed_creations << std::endl;
    
    std::cout << "=========================" << std::endl;
}

// 시스템 종료
void ShutdownSystem() {
    std::cout << "\n=== Shutting down PulseOne Collector ===" << std::endl;
    
    // 드라이버 연결 해제
    std::cout << "Disconnecting drivers..." << std::endl;
    for (auto& driver : g_drivers) {
        if (driver->IsConnected()) {
            std::string protocol_name = ProtocolTypeToString(driver->GetProtocolType());
            driver->Disconnect();
            std::cout << "✓ " << protocol_name << " disconnected" << std::endl;
        }
    }
    g_drivers.clear();
    
    // 시스템 컴포넌트 종료
    if (g_db_manager) {
        g_db_manager->Shutdown();
        std::cout << "✓ Database manager shutdown" << std::endl;
    }
    
    if (g_log_manager) {
        g_log_manager->Info("PulseOne Collector shutdown completed");
        g_log_manager->Shutdown();
        std::cout << "✓ Log manager shutdown" << std::endl;
    }
    
    if (g_config_manager) {
        g_config_manager->Shutdown();
        std::cout << "✓ Configuration manager shutdown" << std::endl;
    }
    
    std::cout << "✓ PulseOne Collector shutdown completed" << std::endl;
}

// 메인 함수
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
        
        // 2. 드라이버 로드 및 연결
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