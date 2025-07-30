/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - 모든 타입 오류 완전 해결
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#include "Core/Application.h"

// ✅ 완전한 타입 정의를 위한 실제 헤더 include
#include "Utils/LogManager.h"      // LogManager 완전 정의
#include "Utils/ConfigManager.h"   // ConfigManager 완전 정의  
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
    , logger_(&::LogManager::getInstance())        // ✅ 전역 네임스페이스
    , config_manager_(&::ConfigManager::getInstance())  // ✅ 전역 네임스페이스
    , db_manager_(&::DatabaseManager::getInstance())
    , repository_factory_(&Database::RepositoryFactory::getInstance())
    , worker_factory_(&Workers::WorkerFactory::getInstance()) {
    
    std::cout << "🔧 CollectorApplication 생성됨" << std::endl;
}

CollectorApplication::~CollectorApplication() {
    Cleanup();
    std::cout << "🗑️ CollectorApplication 정리 완료" << std::endl;
}

void CollectorApplication::Run() {
    std::cout << "🚀 PulseOne Collector v2.0 시작 중..." << std::endl;
    
    try {
        // 1. 시스템 초기화
        if (!Initialize()) {
            std::cout << "❌ 초기화 실패" << std::endl;
            return;
        }
        
        // 2. 실행 상태 설정
        is_running_.store(true);
        std::cout << "✅ PulseOne Collector 시작 완료" << std::endl;
        
        // 3. 메인 루프
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
        // 1. WorkerFactory 기본 초기화
        if (!worker_factory_->Initialize()) {
            logger_->Error("Failed to initialize WorkerFactory");
            return false;
        }
        
        // 2. Repository 의존성 주입
        auto device_repo = repository_factory_->getDeviceRepository();
        auto datapoint_repo = repository_factory_->getDataPointRepository();
        
        if (!device_repo) {
            logger_->Error("Failed to get DeviceRepository from RepositoryFactory");
            return false;
        }
        
        if (!datapoint_repo) {
            logger_->Error("Failed to get DataPointRepository from RepositoryFactory");
            return false;
        }
        
        worker_factory_->SetDeviceRepository(device_repo);
        worker_factory_->SetDataPointRepository(datapoint_repo);
        
        // 3. 데이터베이스 클라이언트 의존성 주입 
        // ✅ 이제 WorkerFactory가 전역 타입을 받으므로 간단하게 처리
        auto redis_client_ptr = db_manager_->getRedisClient();    // RedisClient*
        auto influx_client_ptr = db_manager_->getInfluxClient();  // InfluxClient*
        
        if (redis_client_ptr && influx_client_ptr) {
            // ✅ 간단한 shared_ptr 생성 (타입 일치)
            std::shared_ptr<RedisClient> redis_shared(
                redis_client_ptr, 
                [](RedisClient*) {
                    // 빈 deleter - DatabaseManager가 해제 담당
                }
            );
            
            std::shared_ptr<InfluxClient> influx_shared(
                influx_client_ptr,
                [](InfluxClient*) {
                    // 빈 deleter - DatabaseManager가 해제 담당  
                }
            );
            
            worker_factory_->SetDatabaseClients(redis_shared, influx_shared);
            logger_->Info("Database clients injected successfully");
        } else {
            logger_->Warn("Redis or InfluxDB client not available - continuing without them");
            worker_factory_->SetDatabaseClients(nullptr, nullptr);
        }
        
        // 4. 활성 워커들 자동 생성 및 시작
        std::cout << "  🚀 활성 워커들 자동 시작 중..." << std::endl;
        
        auto active_workers = worker_factory_->CreateAllActiveWorkers();
        std::cout << "  📊 총 " << active_workers.size() << "개 워커 생성됨" << std::endl;
        
        logger_->Info("WorkerFactory initialized and all active workers started");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("CollectorApplication::InitializeWorkerFactory() failed: " + std::string(e.what()));
        return false;
    }
}


void CollectorApplication::MainLoop() {
    int loop_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (is_running_.load()) {
        try {
            loop_count++;
            
            // 10초마다 런타임 통계 출력
            if (loop_count % 10 == 0) {
                PrintRuntimeStatistics(loop_count, start_time);
            }
            
            // 1초 대기
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
        } catch (const std::exception& e) {
            logger_->Error("MainLoop exception: " + std::string(e.what()));
            // 예외가 발생해도 루프는 계속 실행
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    
    std::cout << "🔄 메인 루프 종료됨" << std::endl;
}

void CollectorApplication::PrintRuntimeStatistics(int loop_count,
                                                   const std::chrono::steady_clock::time_point& start_time) {
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::minutes>(now - start_time);
    
    std::cout << "\n📊 런타임 통계 (루프: " << loop_count << ", 가동시간: " << uptime.count() << "분)" << std::endl;
    std::cout << "================================" << std::endl;
    
    try {
        // WorkerFactory 통계 - FactoryStats 구조체 직접 접근
        auto factory_stats = worker_factory_->GetFactoryStats();
        std::cout << "🔧 총 워커: " << factory_stats.workers_created << "개" << std::endl;
        std::cout << "✅ 생성 성공: " << factory_stats.workers_created << "개" << std::endl;
        std::cout << "❌ 생성 실패: " << factory_stats.creation_failures << "개" << std::endl;
        
        // ✅ RepositoryFactory 통계 - 존재하는 메서드 직접 사용
        std::cout << "🏭 Repository 생성: " << repository_factory_->getCreationCount() << "개" << std::endl;
        std::cout << "⚠️ Repository 오류: " << repository_factory_->getErrorCount() << "개" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "⚠️ 통계 수집 중 오류: " << e.what() << std::endl;
    }
    
    std::cout << "================================\n" << std::endl;
}

void CollectorApplication::Cleanup() {
    std::cout << "🧹 시스템 정리 중..." << std::endl;
    
    try {
        // 1. 워커들 정리
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