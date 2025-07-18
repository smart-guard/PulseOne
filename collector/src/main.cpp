// =============================================================================
// collector/src/main.cpp
// PulseOne 데이터 수집기 메인 애플리케이션
// 기존 코드 + 새로운 데이터 액세스 아키텍처 통합
// =============================================================================

#include <iostream>
#include <signal.h>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <map>

// 기존 PulseOne 헤더들
#include "Config/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Utils/LogManager.h"

// 새로운 데이터 액세스 아키텍처
#include "Database/DataAccessManager.h"
#include "Database/DeviceDataAccess.h"
#include "Engine/DeviceIntegration.h"
#include "Engine/DeviceWorker.h"  // DeviceWorker 헤더 (있다고 가정)

// 새로운 드라이버 시스템 헤더들 (있을 때만)
#ifdef HAVE_DRIVER_SYSTEM
#include "Drivers/DriverFactory.h"
#include "Drivers/CommonTypes.h"
#include "Drivers/ModbusDriver.h"
#include "Drivers/BACnetDriver.h"
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
using namespace PulseOne::Database;
using namespace PulseOne::Engine;

// =============================================================================
// 전역 변수들
// =============================================================================
std::atomic<bool> g_running(true);

// 기존 매니저들
std::shared_ptr<ConfigManager> g_config_manager;
std::shared_ptr<LogManager> g_log_manager;

// 새로운 아키텍처 매니저들
std::unique_ptr<DeviceIntegration> g_device_integration;
std::map<int, std::unique_ptr<DeviceWorker>> g_device_workers;

#ifdef HAVE_DRIVER_SYSTEM
std::vector<std::unique_ptr<IProtocolDriver>> g_drivers;
#endif

// =============================================================================
// 시그널 핸들러
// =============================================================================
void SignalHandler(int signal) {
    std::cout << "\nReceived signal " << signal << ". Shutting down gracefully..." << std::endl;
    g_running.store(false);
    
    // DeviceIntegration 즉시 종료 신호
    if (g_device_integration) {
        g_device_integration->Shutdown();
    }
    
    // DeviceWorker들 정지
    for (auto& [device_id, worker] : g_device_workers) {
        if (worker) {
            worker->Stop();
        }
    }
}

// =============================================================================
// 시스템 초기화
// =============================================================================
bool InitializeSystem() {
    try {
        std::cout << "=== PulseOne Collector Starting ===" << std::endl;
        
        // 1. 로그 관리자 초기화 (가장 먼저)
        g_log_manager = std::make_shared<LogManager>();
        if (!g_log_manager->Initialize()) {
            std::cerr << "❌ Failed to initialize LogManager" << std::endl;
            return false;
        }
        std::cout << "✅ Log manager initialized" << std::endl;
        
        // 2. 설정 관리자 초기화
        g_config_manager = std::make_shared<ConfigManager>();
        if (!g_config_manager->Initialize()) {
            std::cerr << "❌ Failed to initialize ConfigManager" << std::endl;
            return false;
        }
        std::cout << "✅ Configuration manager initialized" << std::endl;
        
        // 3. 기존 데이터베이스 관리자 초기화 (먼저!)
        auto& legacy_db = DatabaseManager::getInstance();
        if (!legacy_db.initialize()) {
            std::cerr << "❌ Failed to initialize legacy DatabaseManager" << std::endl;
            return false;
        }
        std::cout << "✅ Legacy database manager initialized" << std::endl;
        
        // 4. 새로운 데이터 액세스 관리자 초기화
        auto& data_access_manager = DataAccessManager::GetInstance();
        if (!data_access_manager.Initialize(g_log_manager, g_config_manager)) {
            std::cerr << "❌ Failed to initialize DataAccessManager" << std::endl;
            return false;
        }
        std::cout << "✅ Data access manager initialized" << std::endl;
        
        // 5. DeviceIntegration 초기화
        g_device_integration = std::make_unique<DeviceIntegration>(g_log_manager, g_config_manager);
        if (!g_device_integration->Initialize()) {
            std::cerr << "❌ Failed to initialize DeviceIntegration" << std::endl;
            return false;
        }
        std::cout << "✅ Device integration layer initialized" << std::endl;
        
#ifdef HAVE_DRIVER_SYSTEM
        // 6. 드라이버 팩토리 초기화 (새 시스템이 있을 때만)
        if (!InitializeDriverFactory()) {
            std::cerr << "❌ Failed to initialize driver factory" << std::endl;
            return false;
        }
        std::cout << "✅ Driver factory initialized" << std::endl;
#else
        std::cout << "✅ Using legacy driver system" << std::endl;
#endif
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ System initialization failed: " << e.what() << std::endl;
        return false;
    }
}

#ifdef HAVE_DRIVER_SYSTEM
// =============================================================================
// 드라이버 팩토리 초기화
// =============================================================================
bool InitializeDriverFactory() {
    try {
        // 사용 가능한 드라이버들을 자동 등록 (컴파일 타임에 이미 등록됨)
        auto& factory = DriverFactory::GetInstance();
        auto available_protocols = factory.GetAvailableProtocols();
        
        g_log_manager->Info("Available protocol drivers:");
        for (const auto& protocol : available_protocols) {
            g_log_manager->Info("  - " + ProtocolTypeToString(protocol));
        }
        
        return true;
        
    } catch (const std::exception& e) {
        g_log_manager->Error("Driver factory initialization failed: " + std::string(e.what()));
        return false;
    }
}
#endif

// =============================================================================
// 디바이스 워커 로드 및 시작
// =============================================================================
bool LoadAndStartDeviceWorkers() {
    try {
        g_log_manager->Info("Loading device configurations and starting workers...");
        
        // DeviceDataAccess를 통해 활성화된 디바이스 목록 조회
        auto& data_access_manager = DataAccessManager::GetInstance();
        auto device_access = data_access_manager.GetDomain<DeviceDataAccess>();
        
        if (!device_access) {
            g_log_manager->Error("Failed to get DeviceDataAccess");
            return false;
        }
        
        // 테넌트 ID 가져오기 (설정에서)
        int tenant_id = std::stoi(g_config_manager->GetValue("tenant_id", "1"));
        auto devices = device_access->GetDevicesByTenant(tenant_id);
        
        g_log_manager->Info("Found " + std::to_string(devices.size()) + " devices for tenant " + std::to_string(tenant_id));
        
        // 각 디바이스에 대해 워커 생성 및 시작
        int workers_started = 0;
        for (const auto& device_info : devices) {
            if (!device_info.is_enabled) {
                g_log_manager->Info("Skipping disabled device: " + device_info.name);
                continue;
            }
            
            try {
                // DeviceWorker 생성 (DeviceIntegration과 연동)
                auto worker = std::make_unique<DeviceWorker>(
                    device_info.id,
                    g_log_manager,
                    g_config_manager,
                    g_device_integration.get()
                );
                
                // 워커 초기화 및 시작
                if (worker->Initialize()) {
                    worker->Start();
                    g_device_workers[device_info.id] = std::move(worker);
                    workers_started++;
                    
                    g_log_manager->Info("Started worker for device: " + device_info.name + 
                                       " (ID: " + std::to_string(device_info.id) + 
                                       ", Protocol: " + device_info.protocol_type + ")");
                } else {
                    g_log_manager->Error("Failed to initialize worker for device: " + device_info.name);
                }
                
            } catch (const std::exception& e) {
                g_log_manager->Error("Failed to create worker for device " + device_info.name + 
                                   ": " + std::string(e.what()));
            }
        }
        
        g_log_manager->Info("Successfully started " + std::to_string(workers_started) + 
                           " device workers out of " + std::to_string(devices.size()) + " devices");
        
        return workers_started > 0;
        
    } catch (const std::exception& e) {
        g_log_manager->Error("LoadAndStartDeviceWorkers failed: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 시스템 통계 출력
// =============================================================================
void PrintSystemStatistics() {
    try {
        std::cout << "\n=== PulseOne System Statistics ===" << std::endl;
        
        // 기본 정보
        std::cout << "Active device workers: " << g_device_workers.size() << std::endl;
        
        // 데이터베이스 연결 상태
        auto& legacy_db = DatabaseManager::getInstance();
        std::cout << "PostgreSQL connected: " << (legacy_db.isPostgresConnected() ? "✅" : "❌") << std::endl;
        std::cout << "SQLite connected: " << (legacy_db.isSQLiteConnected() ? "✅" : "❌") << std::endl;
        
        // DeviceIntegration 통계
        if (g_device_integration) {
            auto stats = g_device_integration->GetProcessingStats();
            std::cout << "Data integration statistics:" << std::endl;
            std::cout << "  Events processed: " << stats["events_processed"] << std::endl;
            std::cout << "  Batch operations: " << stats["batch_operations"] << std::endl;
            std::cout << "  Cache hit rate: " << std::fixed << std::setprecision(2) 
                      << (stats["cache_hit_rate"].get<double>() * 100) << "%" << std::endl;
        }
        
        // 디바이스별 상태
        for (const auto& [device_id, worker] : g_device_workers) {
            if (worker && worker->IsRunning()) {
                std::cout << "Device " << device_id << ": Running ✅" << std::endl;
            } else {
                std::cout << "Device " << device_id << ": Stopped ❌" << std::endl;
            }
        }
        
        std::cout << "===============================" << std::endl;
        
    } catch (const std::exception& e) {
        g_log_manager->Error("PrintSystemStatistics failed: " + std::string(e.what()));
    }
}

// =============================================================================
// 메인 데이터 수집 루프
// =============================================================================
void RunDataCollectionLoop() {
    g_log_manager->Info("Starting main data collection loop...");
    
    auto last_stats_time = std::chrono::steady_clock::now();
    const auto stats_interval = std::chrono::minutes(5); // 5분마다 통계 출력
    
    int loop_count = 0;
    
    while (g_running.load()) {
        try {
            loop_count++;
            
            // 1. 시스템 상태 체크 (1분마다)
            if (loop_count % 60 == 0) {
                // 데이터베이스 연결 상태 체크
                auto& legacy_db = DatabaseManager::getInstance();
                if (!legacy_db.isPostgresConnected() && !legacy_db.isSQLiteConnected()) {
                    g_log_manager->Warning("No database connections available, attempting reconnect...");
                    legacy_db.initialize();
                }
                
                // DeviceIntegration 헬스 체크
                if (g_device_integration) {
                    auto queue_status = g_device_integration->GetQueueStatus();
                    if (queue_status["event_queue_size"].get<size_t>() > 1000) {
                        g_log_manager->Warning("Event queue is getting large: " + 
                                             std::to_string(queue_status["event_queue_size"].get<size_t>()));
                    }
                }
            }
            
            // 2. 정기 통계 출력 (5분마다)
            auto now = std::chrono::steady_clock::now();
            if (now - last_stats_time >= stats_interval) {
                PrintSystemStatistics();
                last_stats_time = now;
            }
            
            // 3. 워커 상태 체크 및 재시작 (필요시)
            for (auto& [device_id, worker] : g_device_workers) {
                if (worker && !worker->IsRunning() && worker->ShouldRestart()) {
                    g_log_manager->Info("Restarting worker for device " + std::to_string(device_id));
                    worker->Start();
                }
            }
            
            // 1초 대기
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
        } catch (const std::exception& e) {
            g_log_manager->Error("Data collection loop error: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(5)); // 에러 시 5초 대기
        }
    }
    
    g_log_manager->Info("Data collection loop ended");
}

// =============================================================================
// 시스템 종료
// =============================================================================
void ShutdownSystem() {
    g_log_manager->Info("Shutting down PulseOne Collector...");
    
    try {
        // 1. DeviceWorker들 정지
        g_log_manager->Info("Stopping device workers...");
        for (auto& [device_id, worker] : g_device_workers) {
            if (worker) {
                worker->Stop();
                g_log_manager->Info("Stopped worker for device " + std::to_string(device_id));
            }
        }
        g_device_workers.clear();
        
        // 2. DeviceIntegration 종료
        if (g_device_integration) {
            g_log_manager->Info("Shutting down device integration...");
            g_device_integration->Shutdown();
            g_device_integration.reset();
        }
        
        // 3. 데이터 액세스 관리자 정리
        g_log_manager->Info("Cleaning up data access manager...");
        auto& data_access_manager = DataAccessManager::GetInstance();
        data_access_manager.Cleanup();
        
        // 4. 기존 매니저들 정리 (순서 중요!)
        g_config_manager.reset();
        
        // 5. 로그 관리자는 마지막에
        if (g_log_manager) {
            g_log_manager->Info("PulseOne Collector shutdown completed");
            g_log_manager.reset();
        }
        
        std::cout << "✅ System shutdown completed successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error during shutdown: " << e.what() << std::endl;
    }
}

// =============================================================================
// 메인 함수
// =============================================================================
int main(int argc, char* argv[]) {
    try {
        // 시그널 핸들러 등록
        signal(SIGINT, SignalHandler);   // Ctrl+C
        signal(SIGTERM, SignalHandler);  // 종료 신호
        
        std::cout << R"(
    ____        __           ____                 
   / __ \__  __/ /________  / __ \____  ___       
  / /_/ / / / / / ___/ _ \/ / / / __ \/ _ \      
 / ____/ /_/ / (__  )  __/ /_/ / / / /  __/      
/_/    \__,_/_/____/\___/\____/_/ /_/\___/       
                                                 
Data Collector v2.0.0 - Enhanced Architecture
)" << std::endl;
        
        std::cout << "Starting initialization..." << std::endl;
        
        // 1. 시스템 초기화
        if (!InitializeSystem()) {
            std::cerr << "❌ System initialization failed!" << std::endl;
            return -1;
        }
        
        // 2. 디바이스 워커들 로드 및 시작
        bool workers_started = LoadAndStartDeviceWorkers();
        
        if (!workers_started) {
            std::cout << "⚠️  No device workers started - running in monitoring mode" << std::endl;
        }
        
        // 3. 초기 상태 출력
        PrintSystemStatistics();
        
        std::cout << "\n🚀 PulseOne Collector is running! Press Ctrl+C to stop.\n" << std::endl;
        
        // 4. 메인 데이터 수집 루프 실행
        RunDataCollectionLoop();
        
        // 5. 정상 종료
        ShutdownSystem();
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 Fatal error: " << e.what() << std::endl;
        
        // 비상 종료
        g_running.store(false);
        ShutdownSystem();
        return -1;
    }
}

// =============================================================================
// 추가 유틸리티 함수들 (필요시)
// =============================================================================

#ifdef HAVE_DRIVER_SYSTEM
// 사용 가능한 프로토콜 드라이버 나열
void ListAvailableDrivers() {
    auto& factory = DriverFactory::GetInstance();
    auto protocols = factory.GetAvailableProtocols();
    
    std::cout << "Available Protocol Drivers:" << std::endl;
    for (const auto& protocol : protocols) {
        std::cout << "  - " << ProtocolTypeToString(protocol) << std::endl;
    }
}
#endif

// 설정 파일 유효성 검사
bool ValidateConfiguration() {
    try {
        // 필수 설정 항목들 확인
        std::vector<std::string> required_configs = {
            "tenant_id",
            "database.host",
            "database.port",
            "database.username",
            "database.password"
        };
        
        for (const auto& config_key : required_configs) {
            std::string value = g_config_manager->GetValue(config_key, "");
            if (value.empty()) {
                g_log_manager->Error("Required configuration missing: " + config_key);
                return false;
            }
        }
        
        return true;
        
    } catch (const std::exception& e) {
        g_log_manager->Error("Configuration validation failed: " + std::string(e.what()));
        return false;
    }
}