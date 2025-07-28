/**
 * @file CollectorApplication.cpp
 * @brief PulseOne Collector 메인 애플리케이션 (완성본)
 * @author PulseOne Development Team
 * @date 2025-07-28
 * 
 * 🎯 추가된 기능:
 * - DatabaseManager 초기화
 * - RepositoryFactory 초기화  
 * - WorkerFactory 초기화
 * - 데이터베이스에서 디바이스 로드
 * - Worker들 자동 생성 및 시작
 */

#include "Core/Application.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/Repositories/RepositoryFactory.h"
#include "Workers/WorkerFactory.h"
#include "DBClient.h"
#include "RedisClient.h"
#include "InfluxClient.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace PulseOne {
namespace Core {

CollectorApplication::CollectorApplication() 
    : logger_(LogManager::getInstance())
    , start_time_(std::chrono::system_clock::now()) {
    
    logger_.Info("🚀 CollectorApplication v" + version_ + " created");
}

CollectorApplication::~CollectorApplication() {
    if (running_.load()) {
        Stop();
    }
    logger_.Info("💀 CollectorApplication destroyed");
}

void CollectorApplication::Run() {
    logger_.Info("🏁 Starting CollectorApplication...");
    
    try {
        if (!Initialize()) {
            logger_.Error("❌ Initialization failed");
            return;
        }
        
        logger_.Info("✅ Initialization successful");
        MainLoop();
        Cleanup();
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Fatal exception: " + std::string(e.what()));
    }
}

bool CollectorApplication::Initialize() {
    try {
        logger_.Info("🔧 PulseOne Collector System 초기화 시작...");
        
        // =======================================================================
        // 1단계: ConfigManager 초기화 (기존)
        // =======================================================================
        if (!InitializeConfigManager()) {
            return false;
        }
        
        // =======================================================================  
        // 2단계: DatabaseManager 초기화 (신규)
        // =======================================================================
        if (!InitializeDatabaseManager()) {
            return false;
        }
        
        // =======================================================================
        // 3단계: RepositoryFactory 초기화 (신규)
        // =======================================================================
        if (!InitializeRepositoryFactory()) {
            return false;
        }
        
        // =======================================================================
        // 4단계: 데이터베이스 클라이언트 초기화 (신규)
        // =======================================================================
        if (!InitializeDatabaseClients()) {
            return false;
        }
        
        // =======================================================================
        // 5단계: WorkerFactory 초기화 (신규)
        // =======================================================================
        if (!InitializeWorkerFactory()) {
            return false;
        }
        
        // =======================================================================
        // 6단계: Worker들 생성 및 시작 (신규)
        // =======================================================================
        if (!InitializeWorkers()) {
            return false;
        }
        
        start_time_ = std::chrono::system_clock::now();
        logger_.Info("🎉 CollectorApplication 초기화 완료!");
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Init exception: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 단계별 초기화 메서드들
// =============================================================================

bool CollectorApplication::InitializeConfigManager() {
    try {
        logger_.Info("🔧 ConfigManager 초기화 시작...");
        
        config_ = &ConfigManager::getInstance();
        config_->initialize();
        
        auto all_config = config_->listAll();
        logger_.Info("📋 Loaded " + std::to_string(all_config.size()) + " configuration entries");
        
        if (all_config.empty()) {
            logger_.Warn("⚠️ No configuration entries loaded - using defaults");
        } else {
            int count = 0;
            for (const auto& [key, value] : all_config) {
                if (count < 5) {
                    logger_.Info("  " + key + " = " + value);
                    count++;
                } else if (count == 5) {
                    logger_.Info("  ... (총 " + std::to_string(all_config.size()) + "개 설정)");
                    break;
                }
            }
        }
        
        // 주요 설정 확인
        std::string db_type = config_->getOrDefault("DATABASE_TYPE", "SQLITE");
        std::string node_env = config_->getOrDefault("NODE_ENV", "development");
        std::string log_level = config_->getOrDefault("LOG_LEVEL", "info");
        
        logger_.Info("✅ ConfigManager 초기화 완료!");
        logger_.Info("   DATABASE_TYPE: " + db_type);
        logger_.Info("   NODE_ENV: " + node_env);
        logger_.Info("   LOG_LEVEL: " + log_level);
        
        std::string config_dir = config_->getConfigDirectory();
        if (!config_dir.empty()) {
            logger_.Info("   CONFIG_DIR: " + config_dir);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 ConfigManager 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeDatabaseManager() {
    try {
        logger_.Info("🗄️ DatabaseManager 초기화 시작...");
        
        // DatabaseManager 싱글톤 획득
        db_manager_ = &PulseOne::Database::DatabaseManager::getInstance();
        
        // 데이터베이스 연결 초기화
        if (!db_manager_->initialize()) {
            logger_.Error("❌ DatabaseManager 초기화 실패");
            return false;
        }
        
        // 연결 테스트
        if (!db_manager_->testConnection()) {
            logger_.Error("❌ Database 연결 테스트 실패");
            return false;
        }
        
        logger_.Info("✅ DatabaseManager 초기화 완료!");
        logger_.Info("   Database 연결 상태: 정상");
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 DatabaseManager 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeRepositoryFactory() {
    try {
        logger_.Info("🏭 RepositoryFactory 초기화 시작...");
        
        // RepositoryFactory 싱글톤 획득 및 초기화
        using namespace PulseOne::Database::Repositories;
        auto& repo_factory = RepositoryFactory::getInstance();
        
        if (!repo_factory.Initialize()) {
            logger_.Error("❌ RepositoryFactory 초기화 실패");
            return false;
        }
        
        // Repository들 획득
        device_repo_ = repo_factory.GetDeviceRepository();
        datapoint_repo_ = repo_factory.GetDataPointRepository();
        
        if (!device_repo_ || !datapoint_repo_) {
            logger_.Error("❌ Repository 획득 실패");
            return false;
        }
        
        // Repository 연결 테스트
        int device_count = device_repo_->getTotalCount();
        int datapoint_count = datapoint_repo_->getTotalCount();
        
        logger_.Info("✅ RepositoryFactory 초기화 완료!");
        logger_.Info("   Device Count: " + std::to_string(device_count));
        logger_.Info("   DataPoint Count: " + std::to_string(datapoint_count));
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 RepositoryFactory 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeDatabaseClients() {
    try {
        logger_.Info("🔌 Database Clients 초기화 시작...");
        
        // Redis 클라이언트 초기화
        try {
            redis_client_ = std::make_shared<RedisClient>();
            logger_.Info("✅ Redis Client 초기화 완료");
        } catch (const std::exception& e) {
            logger_.Warn("⚠️ Redis Client 초기화 실패: " + std::string(e.what()));
            logger_.Warn("   Redis 없이 계속 진행...");
        }
        
        // InfluxDB 클라이언트 초기화
        try {
            influx_client_ = std::make_shared<InfluxClient>();
            logger_.Info("✅ InfluxDB Client 초기화 완료");
        } catch (const std::exception& e) {
            logger_.Warn("⚠️ InfluxDB Client 초기화 실패: " + std::string(e.what()));
            logger_.Warn("   InfluxDB 없이 계속 진행...");
        }
        
        logger_.Info("✅ Database Clients 초기화 완료!");
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Database Clients 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeWorkerFactory() {
    try {
        logger_.Info("🏭 WorkerFactory 초기화 시작...");
        
        // WorkerFactory 싱글톤 획득 및 초기화
        using namespace PulseOne::Workers;
        worker_factory_ = &WorkerFactory::getInstance();
        
        if (!worker_factory_->Initialize()) {
            logger_.Error("❌ WorkerFactory 초기화 실패");
            return false;
        }
        
        // Repository 의존성 주입
        worker_factory_->SetDeviceRepository(device_repo_);
        worker_factory_->SetDataPointRepository(datapoint_repo_);
        
        // 데이터베이스 클라이언트 의존성 주입
        if (redis_client_ && influx_client_) {
            worker_factory_->SetDatabaseClients(redis_client_, influx_client_);
            logger_.Info("   Database clients 주입 완료");
        } else {
            logger_.Warn("   일부 Database clients 누락");
        }
        
        // 지원되는 프로토콜 확인
        auto protocols = worker_factory_->GetSupportedProtocols();
        logger_.Info("✅ WorkerFactory 초기화 완료!");
        logger_.Info("   지원 프로토콜: " + std::to_string(protocols.size()) + "개");
        for (const auto& protocol : protocols) {
            logger_.Info("   - " + protocol);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 WorkerFactory 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeWorkers() {
    try {
        logger_.Info("👷 Workers 초기화 시작...");
        
        // 활성 디바이스들 조회
        auto active_devices = device_repo_->findAllActive(1);  // 테넌트 1
        logger_.Info("📊 활성 디바이스 " + std::to_string(active_devices.size()) + "개 발견");
        
        if (active_devices.empty()) {
            logger_.Warn("⚠️ 활성 디바이스가 없습니다. Worker 생성 건너뜀");
            return true;
        }
        
        // 각 디바이스별 Worker 생성
        int success_count = 0;
        int failure_count = 0;
        
        for (const auto& device : active_devices) {
            try {
                logger_.Debug("🔧 Worker 생성 중: " + device.getName() + " (" + device.getProtocolType() + ")");
                
                auto worker = worker_factory_->CreateWorker(device);
                if (worker) {
                    // Worker 시작 시도
                    auto start_future = worker->Start();
                    
                    // 비동기 시작 (타임아웃 5초)
                    if (start_future.wait_for(std::chrono::seconds(5)) == std::future_status::ready) {
                        bool started = start_future.get();
                        if (started) {
                            running_workers_.push_back(std::move(worker));
                            success_count++;
                            logger_.Info("✅ Worker 시작 성공: " + device.getName());
                        } else {
                            failure_count++;
                            logger_.Error("❌ Worker 시작 실패: " + device.getName());
                        }
                    } else {
                        // 타임아웃 - Worker는 백그라운드에서 계속 시작 시도
                        running_workers_.push_back(std::move(worker));
                        success_count++;
                        logger_.Warn("⏰ Worker 시작 타임아웃 (백그라운드 계속): " + device.getName());
                    }
                } else {
                    failure_count++;
                    logger_.Error("❌ Worker 생성 실패: " + device.getName());
                }
                
            } catch (const std::exception& e) {
                failure_count++;
                logger_.Error("💥 Worker 초기화 예외: " + device.getName() + " - " + std::string(e.what()));
            }
        }
        
        logger_.Info("✅ Workers 초기화 완료!");
        logger_.Info("   성공: " + std::to_string(success_count) + "개");
        logger_.Info("   실패: " + std::to_string(failure_count) + "개");
        logger_.Info("   실행 중: " + std::to_string(running_workers_.size()) + "개");
        
        // WorkerFactory 통계 출력
        auto factory_stats = worker_factory_->GetFactoryStats();
        logger_.Info("📊 WorkerFactory 통계:");
        logger_.Info(factory_stats);
        
        return true;
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Workers 초기화 실패: " + std::string(e.what()));
        return false;
    }
}

// =============================================================================
// 메인 루프 및 상태 관리 (기존 + 개선)
// =============================================================================

void CollectorApplication::MainLoop() {
    running_.store(true);
    int heartbeat_counter = 0;
    
    logger_.Info("🔄 메인 루프 시작 (5초 간격)");
    
    while (running_.load()) {
        try {
            heartbeat_counter++;
            
            // 60초마다 상태 출력
            if (heartbeat_counter % 12 == 0) {
                PrintStatus();
                heartbeat_counter = 0;
            } else {
                logger_.Debug("💗 Heartbeat #" + std::to_string(heartbeat_counter));
            }
            
            // Worker 상태 모니터링 (120초마다)
            if (heartbeat_counter % 24 == 0) {
                MonitorWorkers();
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
        } catch (const std::exception& e) {
            logger_.Error("💥 MainLoop exception: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    logger_.Info("🛑 Main loop stopped");
}

void CollectorApplication::MonitorWorkers() {
    try {
        logger_.Info("👀 Worker 상태 모니터링...");
        
        int running_count = 0;
        int stopped_count = 0;
        int error_count = 0;
        
        for (const auto& worker : running_workers_) {
            if (worker) {
                auto state = worker->GetCurrentState();
                switch (state) {
                    case PulseOne::WorkerState::RUNNING:
                        running_count++;
                        break;
                    case PulseOne::WorkerState::STOPPED:
                        stopped_count++;
                        break;
                    case PulseOne::WorkerState::ERROR:
                        error_count++;
                        logger_.Warn("⚠️ Worker 오류 상태: " + worker->GetDeviceInfo().name);
                        break;
                    default:
                        break;
                }
            }
        }
        
        logger_.Info("📊 Worker 상태: 실행중=" + std::to_string(running_count) + 
                    ", 중지됨=" + std::to_string(stopped_count) + 
                    ", 오류=" + std::to_string(error_count));
        
        // 오류 Worker 재시작 시도 (필요시)
        if (error_count > 0) {
            logger_.Info("🔄 오류 Worker 재시작 시도...");
            // TODO: 오류 Worker 재시작 로직 구현
        }
        
    } catch (const std::exception& e) {
        logger_.Error("💥 MonitorWorkers 예외: " + std::string(e.what()));
    }
}

void CollectorApplication::Stop() {
    logger_.Info("🛑 Stop requested");
    running_.store(false);
    
    // Worker들 정지
    StopAllWorkers();
}

void CollectorApplication::StopAllWorkers() {
    try {
        logger_.Info("🛑 모든 Worker 정지 중...");
        
        int stopped_count = 0;
        for (auto& worker : running_workers_) {
            if (worker) {
                try {
                    auto stop_future = worker->Stop();
                    if (stop_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready) {
                        bool stopped = stop_future.get();
                        if (stopped) {
                            stopped_count++;
                            logger_.Debug("✅ Worker 정지: " + worker->GetDeviceInfo().name);
                        } else {
                            logger_.Warn("⚠️ Worker 정지 실패: " + worker->GetDeviceInfo().name);
                        }
                    } else {
                        logger_.Warn("⏰ Worker 정지 타임아웃: " + worker->GetDeviceInfo().name);
                    }
                } catch (const std::exception& e) {
                    logger_.Error("💥 Worker 정지 예외: " + std::string(e.what()));
                }
            }
        }
        
        logger_.Info("✅ " + std::to_string(stopped_count) + "/" + 
                    std::to_string(running_workers_.size()) + " Workers 정지 완료");
        
        running_workers_.clear();
        
    } catch (const std::exception& e) {
        logger_.Error("💥 StopAllWorkers 예외: " + std::string(e.what()));
    }
}

void CollectorApplication::Cleanup() {
    logger_.Info("🧹 Cleanup started");
    
    try {
        // Worker들 정지 (아직 안 했다면)
        if (!running_workers_.empty()) {
            StopAllWorkers();
        }
        
        // WorkerFactory 정리
        if (worker_factory_) {
            logger_.Debug("🧹 WorkerFactory cleanup");
            auto final_stats = worker_factory_->GetFactoryStats();
            logger_.Info("📊 최종 WorkerFactory 통계:");
            logger_.Info(final_stats);
        }
        
        // Repository 정리
        if (device_repo_ || datapoint_repo_) {
            logger_.Debug("🧹 Repositories cleanup");
            device_repo_.reset();
            datapoint_repo_.reset();
        }
        
        // 데이터베이스 클라이언트 정리
        if (redis_client_ || influx_client_) {
            logger_.Debug("🧹 Database clients cleanup");
            redis_client_.reset();
            influx_client_.reset();
        }
        
        // DatabaseManager 정리
        if (db_manager_) {
            logger_.Debug("🧹 DatabaseManager cleanup");
            // DatabaseManager는 싱글톤이므로 명시적 정리 불필요
        }
        
        // ConfigManager 정리
        if (config_) {
            logger_.Debug("🧹 ConfigManager cleanup");
            // ConfigManager는 싱글톤이므로 명시적 정리 불필요
        }
        
        logger_.Info("✅ Cleanup completed successfully");
        
    } catch (const std::exception& e) {
        logger_.Error("💥 Cleanup exception: " + std::string(e.what()));
    }
}

void CollectorApplication::PrintStatus() {
    try {
        auto now = std::chrono::system_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        
        auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime % std::chrono::hours(1));
        auto seconds = uptime % std::chrono::minutes(1);
        
        std::stringstream status;
        status << "📊 STATUS: v" << version_ 
               << " | Uptime: " << hours.count() << "h " 
               << minutes.count() << "m " << seconds.count() << "s"
               << " | Workers: " << running_workers_.size();
        
        logger_.Info(status.str());
        
        // 현재 설정 상태 출력
        if (config_) {
            std::string db_type = config_->getOrDefault("DATABASE_TYPE", "unknown");
            std::string node_env = config_->getOrDefault("NODE_ENV", "unknown");
            std::string data_dir = config_->getOrDefault("DATA_DIR", "unknown");
            
            logger_.Info("📋 Config: DB=" + db_type + ", ENV=" + node_env + ", DATA=" + data_dir);
        } else {
            logger_.Warn("⚠️ ConfigManager not available");
        }
        
        // WorkerFactory 통계 (간단버전)
        if (worker_factory_) {
            auto stats = worker_factory_->GetFactoryStats();
            logger_.Info("🏭 Factory: Created=" + std::to_string(stats.workers_created) + 
                        ", Failures=" + std::to_string(stats.creation_failures));
        }
        
    } catch (const std::exception& e) {
        logger_.Error("💥 PrintStatus exception: " + std::string(e.what()));
    }
}

} // namespace Core
} // namespace PulseOne