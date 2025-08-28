/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - 극도로 단순화된 버전
 */

#include "Core/Application.h"

#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Workers/WorkerFactory.h"
#include "Workers/WorkerManager.h"
#include "Common/Structs.h"

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
        LogManager::getInstance().Info("System initialization starting...");
        
        // 1. 설정 관리자 초기화 (자동 초기화되지만 명시적 호출)
        try {
            ConfigManager::getInstance().initialize();
            LogManager::getInstance().Info("ConfigManager initialized");
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("ConfigManager initialization failed: " + std::string(e.what()));
            return false;
        }
        
        // 2. 데이터베이스 관리자 초기화
        if (!DatabaseManager::getInstance().initialize()) {
            LogManager::getInstance().Error("DatabaseManager initialization failed");
            return false;
        }
        LogManager::getInstance().Info("DatabaseManager initialized");
        
        // 3. Repository 팩토리 초기화
        if (!Database::RepositoryFactory::getInstance().initialize()) {
            LogManager::getInstance().Error("RepositoryFactory initialization failed");
            return false;
        }
        LogManager::getInstance().Info("RepositoryFactory initialized");
        
        try {
            LogManager::getInstance().Info("Starting all active workers...");
            int started_count = Workers::WorkerManager::getInstance().StartAllActiveWorkers();
            
            if (started_count > 0) {
                LogManager::getInstance().Info("Started " + std::to_string(started_count) + " active workers");
            } else {
                LogManager::getInstance().Warn("No active workers started - check device configuration");
            }
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Worker startup failed: " + std::string(e.what()));
            return false;
        }
        
        if (!InitializeRestApiServer()) {
            LogManager::getInstance().Error("RestApiServer initialization failed");
            return false;
        }

        LogManager::getInstance().Info("System initialization completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Initialization failed: " + std::string(e.what()));
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
                auto manager_stats = worker_manager.GetManagerStats();
                
                LogManager::getInstance().Info("Health check: " + std::to_string(active_count) + " active workers");
                LogManager::getInstance().Debug("Manager Stats: " + manager_stats.dump());
                
                last_health_check = now;
            }
            
            // 주기적 통계 리포트 (1시간마다)
            if (now - last_stats_report >= stats_report_interval) {
                LogManager::getInstance().Info("=== Hourly Statistics Report ===");
                
                // WorkerManager 통계
                auto manager_stats = worker_manager.GetManagerStats();
                LogManager::getInstance().Info("WorkerManager Stats:");
                LogManager::getInstance().Info("  Active Workers: " + std::to_string(manager_stats["active_workers"]));
                LogManager::getInstance().Info("  Total Started: " + std::to_string(manager_stats["total_started"]));
                LogManager::getInstance().Info("  Total Stopped: " + std::to_string(manager_stats["total_stopped"]));
                LogManager::getInstance().Info("  Control Commands: " + std::to_string(manager_stats["total_control_commands"]));
                LogManager::getInstance().Info("  Total Errors: " + std::to_string(manager_stats["total_errors"]));
                
                // WorkerFactory 통계
                auto& worker_factory = Workers::WorkerFactory::getInstance();
                LogManager::getInstance().Info("WorkerFactory Stats:");
                LogManager::getInstance().Info("  " + worker_factory.GetFactoryStatsString());
                
                LogManager::getInstance().Info("=== End Statistics Report ===");
                
                last_stats_report = now;
            }
            
            // 메인 루프 간격 (30초)
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
        } catch (const std::exception& e) {
            LogManager::getInstance().Error("Exception in MainLoop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    LogManager::getInstance().Info("Main loop ended");
}

void CollectorApplication::Cleanup() {
    LogManager::getInstance().Info("System cleanup starting...");
    
    try {
        is_running_.store(false);
        
        // REST API 서버 정리
#ifdef HAVE_HTTPLIB
        if (api_server_) {
            LogManager::getInstance().Info("Stopping REST API server...");
            api_server_->Stop();
            api_server_.reset();
        }
#endif
        
        // WorkerManager를 통한 모든 워커 중지
        LogManager::getInstance().Info("Stopping all workers...");
        Workers::WorkerManager::getInstance().StopAllWorkers();
        
        LogManager::getInstance().Info("System cleanup completed");
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception in Cleanup: " + std::string(e.what()));
    }
}

bool CollectorApplication::InitializeRestApiServer() {
#ifdef HAVE_HTTPLIB
    try {
        api_server_ = std::make_unique<Network::RestApiServer>(8080);
        
        // RestApiServer가 자체적으로 WorkerManager 콜백 설정
        api_server_->SetupWorkerCallbacks(&Workers::WorkerManager::getInstance());
        
        // 설정 API 콜백 등록
        PulseOne::Api::ConfigApiCallbacks::Setup(
            api_server_.get(), 
            &ConfigManager::getInstance(), 
            &LogManager::getInstance()
        );
        
        // 디바이스 API 콜백 등록
        PulseOne::Api::DeviceApiCallbacks::Setup(
            api_server_.get(),
            &Workers::WorkerManager::getInstance(),
            &LogManager::getInstance()
        );
        
        if (api_server_->Start()) {
            LogManager::getInstance().Info("REST API Server started on port 8080");
            return true;
        } else {
            LogManager::getInstance().Error("Failed to start REST API Server");
            return false;
        }
        
    } catch (const std::exception& e) {
        LogManager::getInstance().Error("Exception in InitializeRestApiServer: " + std::string(e.what()));
        return false;
    }
#else
    LogManager::getInstance().Info("REST API Server disabled - HTTP library not available");
    return true;
#endif
}

} // namespace Core
} // namespace PulseOne