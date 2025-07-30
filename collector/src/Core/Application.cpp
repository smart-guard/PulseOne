/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - shared_ptr 문제 완전 해결
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
    , worker_factory_(&PulseOne::Workers::WorkerFactory::getInstance()) {  // ✅ 올바른 네임스페이스
    
    std::cout << "🔧 CollectorApplication 생성됨" << std::endl;
}

CollectorApplication::~CollectorApplication() {
    Cleanup();
    std::cout << "🗑️ CollectorApplication 정리 완료" << std::endl;
}

void CollectorApplication::Run() {
    std::cout << "🚀 PulseOne Collector v2.0 시작 중..." << std::endl;
    
    try {
        if (!Initialize()) {
            std::cout << "❌ 초기화 실패" << std::endl;
            return;
        }
        
        is_running_.store(true);
        std::cout << "✅ PulseOne Collector 시작 완료" << std::endl;
        
        MainLoop();
        
    } catch (const std::exception& e) {
        std::cout << "💥 실행 중 오류: " << e.what() << std::endl;
        logger_->Error("CollectorApplication::Run() failed: " + std::string(e.what()));
    }
    
    std::cout << "🛑 PulseOne Collector 종료됨" << std::endl;
}

void CollectorApplication::Stop() {
    std::cout << "🛑 종료 요청 받음..." << std::endl;
    is_running_.store(false);
}

bool CollectorApplication::Initialize() {
    try {
        std::cout << "📋 시스템 초기화 중..." << std::endl;
        
        // 1. 설정 관리자 초기화
        std::cout << "  📋 ConfigManager 초기화..." << std::endl;
        try {
            config_manager_->initialize();
            std::cout << "  ✅ ConfigManager 초기화 완료" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  ❌ ConfigManager 초기화 실패: " << e.what() << std::endl;
            return false;
        }
        
        // 2. 데이터베이스 관리자 초기화
        std::cout << "  🗄️ DatabaseManager 초기화..." << std::endl;
        if (!db_manager_->initialize()) {
            std::cout << "  ❌ DatabaseManager 초기화 실패" << std::endl;
            return false;
        }
        std::cout << "  ✅ DatabaseManager 초기화 완료" << std::endl;
        
        // 3. Repository 팩토리 초기화
        std::cout << "  🏭 RepositoryFactory 초기화..." << std::endl;
        if (!repository_factory_->initialize()) {
            std::cout << "  ❌ RepositoryFactory 초기화 실패" << std::endl;
            return false;
        }
        std::cout << "  ✅ RepositoryFactory 초기화 완료" << std::endl;
        
        // 4. WorkerFactory 초기화
        std::cout << "  ⚙️ WorkerFactory 초기화..." << std::endl;
        if (!InitializeWorkerFactory()) {
            std::cout << "  ❌ WorkerFactory 초기화 실패" << std::endl;
            return false;
        }
        std::cout << "  ✅ WorkerFactory 초기화 완료" << std::endl;
        
        std::cout << "✅ 시스템 초기화 완료!" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "💥 초기화 중 예외: " << e.what() << std::endl;
        logger_->Error("CollectorApplication::Initialize() failed: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeWorkerFactory() {
    try {
        // ✅ 1. 기존 코드와 동일하게 Initialize() 호출 (매개변수 없음)
        if (!worker_factory_->Initialize()) {
            logger_->Error("Failed to initialize WorkerFactory");
            return false;
        }
        
        // 2. Repository 의존성 주입
        auto device_repo = repository_factory_->getDeviceRepository();
        auto datapoint_repo = repository_factory_->getDataPointRepository();
        
        if (!device_repo || !datapoint_repo) {
            logger_->Error("Failed to get repositories from RepositoryFactory");
            return false;
        }
        
        worker_factory_->SetDeviceRepository(device_repo);
        worker_factory_->SetDataPointRepository(datapoint_repo);
        
        // ✅ 3. shared_ptr 문제 완전 해결 - 가장 간단한 방법: 그냥 단순 포인터로 래핑
        auto redis_client_raw = db_manager_->getRedisClient();
        auto influx_client_raw = db_manager_->getInfluxClient();
        
        if (!redis_client_raw || !influx_client_raw) {
            logger_->Error("Failed to get database clients from DatabaseManager");
            return false;
        }
        
        // ✅ 해결책: std::shared_ptr 생성자 직접 사용하지 말고 간접적으로
        struct NoDelete {
            void operator()(void*) {} // 아무것도 삭제하지 않는 deleter
        };

        std::shared_ptr<::RedisClient> redis_shared(redis_client_raw, [](::RedisClient*){});
        std::shared_ptr<::InfluxClient> influx_shared(influx_client_raw, [](::InfluxClient*){});

        worker_factory_->SetDatabaseClients(redis_shared, influx_shared);
        
        logger_->Info("✅ WorkerFactory initialization completed");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in InitializeWorkerFactory: " + std::string(e.what()));
        return false;
    }
}

void CollectorApplication::MainLoop() {
    logger_->Info("🔄 메인 루프 시작");
    
    auto start_time = std::chrono::steady_clock::now();
    int loop_count = 0;
    
    while (is_running_.load()) {
        try {
            loop_count++;
            
            // 기본 동작들
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            // 통계 출력 (10초마다)
            if (loop_count % 10 == 0) {
                PrintRuntimeStatistics(loop_count, start_time);
            }
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in main loop: " + std::string(e.what()));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    logger_->Info("🔄 메인 루프 종료");
}

void CollectorApplication::PrintRuntimeStatistics(int loop_count, const std::chrono::steady_clock::time_point& start_time) {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    
    std::stringstream ss;
    ss << "📊 Runtime Statistics (Loop: " << loop_count << ", Uptime: " << uptime.count() << "s)";
    
    // WorkerFactory 통계 추가
    if (worker_factory_) {
        ss << "\n   " << worker_factory_->GetFactoryStatsString();
    }
    
    logger_->Info(ss.str());
}

void CollectorApplication::Cleanup() {
    try {
        std::cout << "🧹 시스템 정리 중..." << std::endl;
        
        is_running_.store(false);
        
        if (worker_factory_) {
            std::cout << "  🔧 WorkerFactory 정리 중..." << std::endl;
            // WorkerFactory는 싱글톤이므로 명시적 정리 불필요
        }
        
        std::cout << "✅ 시스템 정리 완료" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "💥 정리 중 예외: " << e.what() << std::endl;
        logger_->Error("CollectorApplication::Cleanup() failed: " + std::string(e.what()));
    }
}

} // namespace Core
} // namespace PulseOne