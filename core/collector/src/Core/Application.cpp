/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - 실제 메서드명과 일치하도록 수정
 */

#include "Core/Application.h"

#include "Logging/LogManager.h"
#include "Utils/ConfigManager.h"
#include "DatabaseManager.hpp"
#include "Database/RepositoryFactory.h"
#include "Workers/WorkerManager.h"
#include "Alarm/AlarmStartupRecovery.h"


#ifdef HAVE_HTTPLIB
#include "Api/ConfigApiCallbacks.h"
#include "Api/DeviceApiCallbacks.h"
#include "Api/HardwareApiCallbacks.h"
#include "Api/LogApiCallbacks.h"
#endif

#include <iostream>
#include <thread>
#include <chrono>

namespace PulseOne {
namespace Core {

CollectorApplication::CollectorApplication() 
    : is_running_(false) {
    LogManager::getInstance().Info("CollectorApplication initialized");
}

CollectorApplication::~CollectorApplication() {
    Cleanup();
    LogManager::getInstance().Info("CollectorApplication destroyed");
}

void CollectorApplication::Run() {
    LogManager::getInstance().Info("PulseOne Collector v2.0 starting...");
    
    try {
        if (!Initialize()) {
            LogManager::getInstance().Error("Initialization failed");
            return;
        }
        
        is_running_.store(true);
        LogManager::getInstance().Info("PulseOne Collector started successfully");
        
        MainLoop();
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Runtime error: " + std::string(e.what()));
    }
    
    LogManager::getInstance().Info("PulseOne Collector shutdown complete");
}

void CollectorApplication::Stop() {
    LogManager::getInstance().Info("Shutdown requested");
    is_running_.store(false);
}

bool CollectorApplication::Initialize() {
    try {
        LogManager::getInstance().Info("=== SYSTEM INITIALIZATION STARTING ===");
        
        // 1. 설정 관리자 초기화
        try {
            LogManager::getInstance().Info("Step 1/5: Initializing ConfigManager...");
            ConfigManager::getInstance().initialize();
            LogManager::getInstance().Info("✓ ConfigManager initialized successfully");
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("✗ ConfigManager initialization failed: " + std::string(e.what()));
            return false;
        }
        
        // 2. 데이터베이스 관리자 초기화
        LogManager::getInstance().Info("Step 2/5: Database initialization deferred to RepositoryFactory");
        // Initialization is handled by RepositoryFactory to consolidate config loading logic
        LogManager::getInstance().Info("✓ DbLib::DatabaseManager will be initialized in Step 3");
        
        // 3. Repository 팩토리 초기화
        LogManager::getInstance().Info("Step 3/5: Initializing RepositoryFactory...");
        if (!Database::RepositoryFactory::getInstance().initialize()) {
            LogManager::getInstance().Error("✗ RepositoryFactory initialization failed");
            return false;
        }
        LogManager::getInstance().Info("✓ RepositoryFactory initialized successfully");
        
        // 4. REST API 서버 초기화
        LogManager::getInstance().Info("Step 4/7: Initializing REST API Server...");
        if (!InitializeRestApiServer()) {
            LogManager::getInstance().Warn("Continuing without REST API server...");
        } else {
            LogManager::getInstance().Info("✓ REST API Server initialized successfully");
        }
        
        // ✅ 5. 포인트 값 복구 (Warm Startup 핵심 로직)
        LogManager::getInstance().Info("Step 5/7: Recovering latest point values (RDB -> Redis -> RAM)...");
        try {
            auto& alarm_recovery = Alarm::AlarmStartupRecovery::getInstance();
            size_t point_count = alarm_recovery.RecoverLatestPointValues();
            LogManager::getInstance().Info("✓ Successfully recovered " + std::to_string(point_count) + " point values");
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("✗ Point value recovery failed: " + std::string(e.what()));
        }

        // ✅ 6. 활성 알람 복구
        LogManager::getInstance().Info("Step 6/7: Recovering active alarms...");
        try {
            auto& alarm_recovery = Alarm::AlarmStartupRecovery::getInstance();
            if (alarm_recovery.IsRecoveryEnabled()) {
                size_t recovered_count = alarm_recovery.RecoverActiveAlarms();
                LogManager::getInstance().Info("✓ Successfully recovered " + std::to_string(recovered_count) + " active alarms");
            }
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("✗ Alarm recovery failed: " + std::string(e.what()));
        }

        // 7. WorkerManager를 통한 활성 워커들 시작
        LogManager::getInstance().Info("Step 7/7: Starting active workers...");
        try {
            auto& worker_manager = Workers::WorkerManager::getInstance();
            int started_count = worker_manager.StartAllActiveWorkers();
            LogManager::getInstance().Info("✓ Started " + std::to_string(started_count) + " active workers");
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("✗ Worker startup failed: " + std::string(e.what()));
        }

        LogManager::getInstance().Info("=== SYSTEM INITIALIZATION COMPLETED ===");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("=== INITIALIZATION FAILED ===");
        LogManager::getInstance().Error("Exception: " + std::string(e.what()));
        return false;
    }
}

void CollectorApplication::MainLoop() {
    LogManager::getInstance().Info("Main loop started - production mode");
    
    auto last_health_check = std::chrono::steady_clock::now();
    auto last_stats_report = std::chrono::steady_clock::now();
    
    const auto health_check_interval = std::chrono::minutes(5);
    const auto stats_report_interval = std::chrono::hours(1);
    
    while (is_running_.load()) {
        try {
            auto now = std::chrono::steady_clock::now();
            auto& worker_manager = Workers::WorkerManager::getInstance();
            
            // 주기적 헬스체크 (5분마다)
            if (now - last_health_check >= health_check_interval) {
                size_t active_count = worker_manager.GetActiveWorkerCount();
                
                LogManager::getInstance().Info("=== Health Check ===");
                LogManager::getInstance().Info("Active workers: " + std::to_string(active_count));
                
                last_health_check = now;
            }
            
            // 주기적 통계 리포트 (1시간마다)
            if (now - last_stats_report >= stats_report_interval) {
                LogManager::getInstance().Info("=== HOURLY STATISTICS REPORT ===");
                
                try {
                    // WorkerManager 통계
                    auto& worker_manager = Workers::WorkerManager::getInstance();
                    size_t active_workers = worker_manager.GetActiveWorkerCount();
                    
                    LogManager::getInstance().Info("WorkerManager Statistics:");
                    LogManager::getInstance().Info("  Active Workers: " + std::to_string(active_workers));
                    
                    // 시스템 리소스 정보
                    LogManager::getInstance().Info("System Status:");
                    
                    // DbLib::DatabaseManager 연결 상태 확인 (enum class 사용)
                    bool db_connected = false;
                    try {
                        db_connected = DbLib::DatabaseManager::getInstance().isConnected(DbLib::DatabaseManager::DatabaseType::POSTGRESQL) ||
                                     DbLib::DatabaseManager::getInstance().isConnected(DbLib::DatabaseManager::DatabaseType::SQLITE);
                    } catch (...) {
                        db_connected = false;
                    }
                    LogManager::getInstance().Info("  Database: " + std::string(db_connected ? "Connected" : "Disconnected"));
                    
                    // RepositoryFactory 상태
                    LogManager::getInstance().Info("  Repositories: " + 
                        std::string(Database::RepositoryFactory::getInstance().isInitialized() ? "Ready" : "Not Ready"));
                    
#ifdef HAVE_HTTPLIB
                    // API 서버 상태
                    LogManager::getInstance().Info("  REST API: " + 
                        std::string((api_server_ && api_server_->IsRunning()) ? "Running" : "Stopped"));
#else
                    LogManager::getInstance().Info("  REST API: Disabled");
#endif
                    
                } catch (const std::exception& e) {
                    LogManager::getInstance().Error("Error generating statistics: " + std::string(e.what()));
                }
                
                LogManager::getInstance().Info("=== END STATISTICS REPORT ===");
                last_stats_report = now;
            }
            
            // 메인 루프 간격 (30초)
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Exception in MainLoop: " + std::string(e.what()));
            // 예외 발생시 잠시 대기 후 계속 실행
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    LogManager::getInstance().Info("Main loop ended");
}

void CollectorApplication::Cleanup() {
    LogManager::getInstance().Info("=== SYSTEM CLEANUP STARTING ===");
    
    try {
        is_running_.store(false);
        
        // 1. REST API 서버 정리
#ifdef HAVE_HTTPLIB
        if (api_server_) {
            LogManager::getInstance().Info("Step 1/3: Stopping REST API server...");
            api_server_->Stop();
            api_server_.reset();
            LogManager::getInstance().Info("✓ REST API server stopped");
        }
#endif
        
        // 2. WorkerManager를 통한 모든 워커 중지
        LogManager::getInstance().Info("Step 2/3: Stopping all workers...");
        try {
            Workers::WorkerManager::getInstance().StopAllWorkers();
            LogManager::getInstance().Info("✓ All workers stopped");
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Error stopping workers: " + std::string(e.what()));
        }
        
        // 3. 데이터베이스 연결 정리 (자동으로 소멸자에서 처리됨)
        LogManager::getInstance().Info("Step 3/3: Database cleanup...");
        try {
            // DbLib::DatabaseManager와 RepositoryFactory는 싱글톤이므로 명시적 정리 불필요
            // 소멸자에서 자동으로 연결 해제됨
            LogManager::getInstance().Info("✓ Database cleanup completed");
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Error in database cleanup: " + std::string(e.what()));
        }
        
        LogManager::getInstance().Info("=== SYSTEM CLEANUP COMPLETED ===");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception in Cleanup: " + std::string(e.what()));
    }
}

bool CollectorApplication::InitializeRestApiServer() {
#ifdef HAVE_HTTPLIB
    try {
        // ConfigManager에서 API 포트 읽기
        int api_port = 8080;  // 기본값
        try {
            api_port = ConfigManager::getInstance().getInt("api.port", 8080);
        } catch (const std::exception& e) {
            LogManager::getInstance().Warn("Could not read API port from config, using default 8080: " + std::string(e.what()));
        }
        
        api_server_ = std::make_unique<Network::RestApiServer>(api_port);
        
        // API 콜백들 등록 - 싱글톤 직접 접근 방식
        LogManager::getInstance().Info("Registering API callbacks...");
        
        // 설정 관련 API 콜백 등록
        PulseOne::Api::ConfigApiCallbacks::Setup(api_server_.get());
        LogManager::getInstance().Info("✓ ConfigApiCallbacks registered");
        
        // 디바이스 제어 관련 API 콜백 등록
        PulseOne::Api::DeviceApiCallbacks::Setup(api_server_.get());
        LogManager::getInstance().Info("✓ DeviceApiCallbacks registered");
        
        PulseOne::Api::HardwareApiCallbacks::Setup(api_server_.get());
        LogManager::getInstance().Info("✓ HardwareApiCallbacks registered");

        PulseOne::Api::LogApiCallbacks::Setup(api_server_.get());
        LogManager::getInstance().Info("✓ LogApiCallbacks registered");
        // API 서버 시작
        if (api_server_->Start()) {
            LogManager::getInstance().Info("✓ REST API Server started on port " + std::to_string(api_port));
            LogManager::getInstance().Info("  API Documentation: http://localhost:" + std::to_string(api_port) + "/api/docs");
            LogManager::getInstance().Info("  Health Check: http://localhost:" + std::to_string(api_port) + "/api/health");
            return true;
        } else {
            LogManager::getInstance().Error("✗ Failed to start REST API Server on port " + std::to_string(api_port));
            return false;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception in InitializeRestApiServer: " + std::string(e.what()));
        return false;
    }
#else
    LogManager::getInstance().Info("REST API Server disabled - HTTP library not available");
    LogManager::getInstance().Info("To enable REST API, compile with -DHAVE_HTTPLIB and link against httplib");
    return true;  // HTTP 라이브러리 없는 것은 에러가 아님
#endif
}

} // namespace Core
} // namespace PulseOne