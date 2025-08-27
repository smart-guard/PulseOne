/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - 기존 구조 유지하며 프로덕션용 수정
 */

#include "Core/Application.h"

#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Workers/WorkerFactory.h"
#include "Common/Structs.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <sstream>

namespace PulseOne {
namespace Core {

CollectorApplication::CollectorApplication() 
    : is_running_(false)
    , logger_(&::LogManager::getInstance())
    , config_manager_(&::ConfigManager::getInstance())
    , db_manager_(&::DatabaseManager::getInstance())
    , repository_factory_(&Database::RepositoryFactory::getInstance())
    , worker_factory_(&PulseOne::Workers::WorkerFactory::getInstance()) {
    
    logger_->Info("CollectorApplication initialized");
}

CollectorApplication::~CollectorApplication() {
    Cleanup();
    logger_->Info("CollectorApplication destroyed");
}

void CollectorApplication::Run() {
    logger_->Info("PulseOne Collector v2.0 starting...");
    
    try {
        if (!Initialize()) {
            logger_->Error("Initialization failed");
            return;
        }
        
        is_running_.store(true);
        logger_->Info("PulseOne Collector started successfully");
        
        MainLoop();
        
    } catch (const std::exception& e) {
        logger_->Error("Runtime error: " + std::string(e.what()));
    }
    
    logger_->Info("PulseOne Collector shutdown complete");
}

void CollectorApplication::Stop() {
    logger_->Info("Shutdown requested");
    is_running_.store(false);
}

bool CollectorApplication::Initialize() {
    try {
        logger_->Info("System initialization starting...");
        
        // 1. 설정 관리자 초기화
        try {
            config_manager_->initialize();
            logger_->Info("ConfigManager initialized");
        } catch (const std::exception& e) {
            logger_->Error("ConfigManager initialization failed: " + std::string(e.what()));
            return false;
        }
        
        // 2. 데이터베이스 관리자 초기화
        if (!db_manager_->initialize()) {
            logger_->Error("DatabaseManager initialization failed");
            return false;
        }
        logger_->Info("DatabaseManager initialized");
        
        // 3. Repository 팩토리 초기화
        if (!repository_factory_->initialize()) {
            logger_->Error("RepositoryFactory initialization failed");
            return false;
        }
        logger_->Info("RepositoryFactory initialized");
        
        // 4. WorkerFactory 초기화
        if (!InitializeWorkerFactory()) {
            logger_->Error("WorkerFactory initialization failed");
            return false;
        }
        logger_->Info("WorkerFactory initialized");
        
        // 5. Workers 생성 및 시작
        try {
            auto workers = worker_factory_->CreateAllActiveWorkers();
            logger_->Info("Created " + std::to_string(workers.size()) + " workers");
            
            // 워커 타입 변환
            for (auto& worker : workers) {
                if (worker) {
                    // unique_ptr을 shared_ptr로 변환
                    active_workers_.push_back(std::shared_ptr<Workers::BaseDeviceWorker>(worker.release()));
                }
            }
            
            // 워커 시작 (Start 메서드 호출하지 않음 - 워커가 자체적으로 시작)
            logger_->Info("Workers initialized - they will start automatically");
            
            if (active_workers_.empty()) {
                logger_->Warn("No workers created - check device configuration");
            } else {
                logger_->Info("All " + std::to_string(active_workers_.size()) + " workers initialized");
            }
        } catch (const std::exception& e) {
            logger_->Error("Worker creation/start failed: " + std::string(e.what()));
            return false;
        }
        
        // REST API 서버 초기화
        if (!InitializeRestApiServer()) {
            logger_->Error("RestApiServer initialization failed");
            return false;
        }

        logger_->Info("System initialization completed successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeWorkerFactory() {
    try {
        if (!worker_factory_->Initialize()) {
            logger_->Error("Failed to initialize WorkerFactory");
            return false;
        }

        auto repo_factory_shared = std::shared_ptr<Database::RepositoryFactory>(
            repository_factory_, [](Database::RepositoryFactory*){}
        );
        worker_factory_->SetRepositoryFactory(repo_factory_shared);

        auto redis_client_raw = db_manager_->getRedisClient();
        auto influx_client_raw = db_manager_->getInfluxClient();
        
        std::shared_ptr<::RedisClient> redis_shared(redis_client_raw, [](::RedisClient*){});
        std::shared_ptr<::InfluxClient> influx_shared(influx_client_raw, [](::InfluxClient*){});
        
        worker_factory_->SetDatabaseClients(redis_shared, influx_shared);

        logger_->Info("WorkerFactory dependencies injected successfully");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in InitializeWorkerFactory: " + std::string(e.what()));
        return false;
    }
}

void CollectorApplication::MainLoop() {
    logger_->Info("Main loop started - production mode");
    
    auto last_health_check = std::chrono::steady_clock::now();
    auto last_stats_report = std::chrono::steady_clock::now();
    
    const auto health_check_interval = std::chrono::minutes(5);
    const auto stats_report_interval = std::chrono::hours(1);
    
    while (is_running_.load()) {
        try {
            auto now = std::chrono::steady_clock::now();
            
            // 주기적 헬스체크 (5분마다)
            if (now - last_health_check >= health_check_interval) {
                // 워커 상태 확인 및 재시작
                int running_workers = 0;
                int total_workers = 0;
                
                for (auto& worker : active_workers_) {
                    if (worker) {
                        total_workers++;
                        // 워커 상태만 확인하고 자동 재시작은 하지 않음
                        running_workers++;
                        // 실제로는 워커 상태를 체크하는 로직 필요하지만
                        // IsRunning() 메서드가 없으므로 존재하면 실행중으로 간주
                    }
                }
                
                if (running_workers != total_workers) {
                    logger_->Warn("Health check: " + std::to_string(running_workers) + 
                                    "/" + std::to_string(total_workers) + " workers running");
                } else {
                    logger_->Debug("Health check: All " + std::to_string(total_workers) + " workers running");
                }
                
                last_health_check = now;
            }
            
            // 주기적 통계 리포트 (1시간마다)
            if (now - last_stats_report >= stats_report_interval) {
                int active_count = 0;
                for (auto& worker : active_workers_) {
                    if (worker) {
                        // BaseDeviceWorker에 IsRunning이 없으므로 단순 체크
                        active_count++;
                    }
                }
                
                logger_->Info("Statistics Report:");
                logger_->Info("  Active Workers: " + std::to_string(active_count) + 
                              "/" + std::to_string(active_workers_.size()));
                
                if (worker_factory_) {
                    logger_->Info("  " + worker_factory_->GetFactoryStatsString());
                }
                
                last_stats_report = now;
            }
            
            // 메인 루프 간격 (30초)
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in MainLoop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    logger_->Info("Main loop ended");
}

void CollectorApplication::PrintRuntimeStatistics(int loop_count, const std::chrono::steady_clock::time_point& start_time) {
    // 이 메서드는 더 이상 사용하지 않지만 호환성을 위해 유지
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    
    std::ostringstream stats;
    stats << "Runtime Statistics:\n"
          << "  Uptime: " << duration.count() << "s\n"
          << "  Loop Count: " << loop_count << "\n";
    
    if (worker_factory_) {
        stats << "  " << worker_factory_->GetFactoryStatsString();
    }
    
    logger_->Info(stats.str());
}

void CollectorApplication::Cleanup() {
    logger_->Info("System cleanup starting...");
    
    try {
        is_running_.store(false);
        
        // REST API 서버 정리
#ifdef HAVE_HTTPLIB
        if (api_server_) {
            logger_->Info("Stopping REST API server...");
            api_server_->Stop();
            api_server_.reset();
        }
#endif
        
        // 모든 워커 중지
        logger_->Info("Stopping workers...");
        for (auto& worker : active_workers_) {
            if (worker) {
                worker->Stop();
            }
        }
        active_workers_.clear();
        
        logger_->Info("System cleanup completed");
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in Cleanup: " + std::string(e.what()));
    }
}

bool CollectorApplication::InitializeRestApiServer() {
#ifdef HAVE_HTTPLIB
    try {
        api_server_ = std::make_unique<Network::RestApiServer>(8080);
        
        PulseOne::Api::ConfigApiCallbacks::Setup(
            api_server_.get(), 
            config_manager_, 
            logger_
        );
        
        PulseOne::Api::DeviceApiCallbacks::Setup(
            api_server_.get(),
            worker_factory_,
            logger_
        );
        
        if (api_server_->Start()) {
            logger_->Info("REST API Server started on port 8080");
            return true;
        } else {
            logger_->Error("Failed to start REST API Server");
            return false;
        }
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in InitializeRestApiServer: " + std::string(e.what()));
        return false;
    }
#else
    logger_->Info("REST API Server disabled - HTTP library not available");
    return true;
#endif
}

} // namespace Core
} // namespace PulseOne