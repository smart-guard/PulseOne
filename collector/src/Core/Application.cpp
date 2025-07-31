/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - WorkerFactory 호출 방식 수정
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
    , worker_factory_(&PulseOne::Workers::WorkerFactory::getInstance()) {  // 🔧 수정: getInstance (소문자 g)
    
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
        std::cout << "  🏭 WorkerFactory 초기화..." << std::endl;
        if (!InitializeWorkerFactory()) {
            std::cout << "  ❌ WorkerFactory 초기화 실패" << std::endl;
            return false;
        }
        std::cout << "  ✅ WorkerFactory 초기화 완료" << std::endl;
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "💥 초기화 중 오류: " << e.what() << std::endl;
        logger_->Error("CollectorApplication::Initialize() failed: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeWorkerFactory() {
    try {
        // 🔧 수정: 매개변수 없는 Initialize() 호출 - 내부에서 싱글톤들 가져옴
        if (!worker_factory_->Initialize()) {
            logger_->Error("Failed to initialize WorkerFactory");
            return false;
        }
        
        // Repository 의존성 주입
        auto device_repo = repository_factory_->getDeviceRepository();
        auto datapoint_repo = repository_factory_->getDataPointRepository();
        
        worker_factory_->SetDeviceRepository(device_repo);
        worker_factory_->SetDataPointRepository(datapoint_repo);
        
        // 🔧 수정: shared_ptr 생성 - 전역 클래스 사용
        auto redis_client_raw = db_manager_->getRedisClient();
        auto influx_client_raw = db_manager_->getInfluxClient();
        
        // 🔧 수정: 올바른 타입으로 shared_ptr 생성
        std::shared_ptr<::RedisClient> redis_shared(redis_client_raw, [](::RedisClient*){});
        std::shared_ptr<::InfluxClient> influx_shared(influx_client_raw, [](::InfluxClient*){});
        
        worker_factory_->SetDatabaseClients(redis_shared, influx_shared);
        
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("Exception in InitializeWorkerFactory: " + std::string(e.what()));
        return false;
    }
}

void CollectorApplication::MainLoop() {
    std::cout << "🔄 메인 루프 시작..." << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    int loop_count = 0;
    bool workers_created = false;
    
    while (is_running_.load()) {
        try {
            loop_count++;
            
            // 🔧 3초 후 Worker 생성 테스트
            if (!workers_created && loop_count == 3) {
                std::cout << "\n🏭 === 실제 Worker 생성 테스트 시작 ===" << std::endl;
                logger_->Info("🏭 Testing worker creation for 5 MODBUS_TCP devices");
                
                auto workers = worker_factory_->CreateAllActiveWorkers();
                
                std::cout << "📊 생성된 Worker 수: " << workers.size() << "/5" << std::endl;
                std::cout << "📊 예상 DataPoint 총합: 16개" << std::endl;
                
                workers_created = true;
                std::cout << "=== Worker 생성 테스트 완료 ===\n" << std::endl;
            }
            
            // 매 10초마다 통계 출력
            if (loop_count % 10 == 0) {
                PrintRuntimeStatistics(loop_count, start_time);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
        } catch (const std::exception& e) {
            logger_->Error("Exception in MainLoop: " + std::string(e.what()));
        }
    }
}

void CollectorApplication::PrintRuntimeStatistics(int loop_count, const std::chrono::steady_clock::time_point& start_time) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    
    std::ostringstream stats;
    stats << "📊 Runtime Statistics:\n"
          << "  Uptime: " << duration.count() << "s\n"
          << "  Loop Count: " << loop_count << "\n";
    
    // WorkerFactory 통계 추가
    if (worker_factory_) {
        stats << "  " << worker_factory_->GetFactoryStatsString();
    }
    
    std::cout << stats.str() << std::endl;
    logger_->Info(stats.str());
}

void CollectorApplication::Cleanup() {
    std::cout << "🧹 시스템 정리 중..." << std::endl;
    
    try {
        is_running_.store(false);
        
        // 필요한 경우 추가 정리 작업
        
        std::cout << "✅ 시스템 정리 완료" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "⚠️ 정리 중 오류: " << e.what() << std::endl;
        logger_->Error("Exception in Cleanup: " + std::string(e.what()));
    }
}

} // namespace Core
} // namespace PulseOne