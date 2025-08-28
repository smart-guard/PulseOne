/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - WorkerManager 사용으로 수정
 */

#include "Core/Application.h"

#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Workers/WorkerManager.h"  // Factory 대신 Manager 사용
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
        
        // 1. 설정 관리자 초기화
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
        
        // 4. WorkerManager를 통한 Worker 시작
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
        
        // 5. REST API 서버 초기화
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
                
                if (manager_stats.contains("active_workers")) {
                    int active_workers = manager_stats["active_workers"].get<int>();
                    LogManager::getInstance().Info("  Active Workers: " + std::to_string(active_workers));
                }
                
                if (manager_stats.contains("total_started")) {
                    int total_started = manager_stats["total_started"].get<int>();
                    LogManager::getInstance().Info("  Total Started: " + std::to_string(total_started));
                }
                
                if (manager_stats.contains("total_stopped")) {
                    int total_stopped = manager_stats["total_stopped"].get<int>();
                    LogManager::getInstance().Info("  Total Stopped: " + std::to_string(total_stopped));
                }
                
                if (manager_stats.contains("total_errors")) {
                    int total_errors = manager_stats["total_errors"].get<int>();
                    LogManager::getInstance().Info("  Total Errors: " + std::to_string(total_errors));
                }
                
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
        
        // 설정 API 콜백 등록
        PulseOne::Api::ConfigApiCallbacks::Setup(
            api_server_.get(), 
            &ConfigManager::getInstance(), 
            &LogManager::getInstance()
        );
        
        // Device API 콜백 등록 - WorkerManager 사용
        PulseOne::Api::DeviceApiCallbacks::Setup(
            api_server_.get(),
            &Workers::WorkerManager::getInstance(),  // WorkerManager 전달
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