/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 - 컴파일 에러 수정 버전
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#include "Core/Application.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Workers/WorkerFactory.h"
#include "Common/UnifiedCommonTypes.h"  // WorkerState enum 포함
#include <iostream>
#include <thread>
#include <chrono>
#include <map>
#include <sstream>

namespace PulseOne {
namespace Core {

CollectorApplication::CollectorApplication() 
    : is_running_(false)
    , logger_(&LogManager::getInstance())
    , config_manager_(&ConfigManager::getInstance())  // 🔧 네임스페이스 수정
    , db_manager_(&DatabaseManager::getInstance())
    , repository_factory_(&Database::RepositoryFactory::getInstance())  // 🔧 네임스페이스 수정
    , worker_factory_(&Workers::WorkerFactory::getInstance()) {  // 🔧 네임스페이스 수정
    
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
        
        // 2. 데이터베이스에서 워커들 로드 및 생성
        if (!LoadWorkersFromDatabase()) {
            std::cout << "❌ 워커 로드 실패" << std::endl;
            return;
        }
        
        // 3. 워커들 시작
        StartWorkers();
        
        // 4. 실행 상태 설정
        is_running_.store(true);
        std::cout << "✅ PulseOne Collector 시작 완료" << std::endl;
        std::cout << "📊 총 활성 워커: " << workers_.size() << "개" << std::endl;
        std::cout << "🔄 메인 루프 실행 중... (Ctrl+C로 종료)" << std::endl;
        
        // 5. 메인 루프
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
        
        // 2. 데이터베이스 매니저 초기화
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
        
        // 4. WorkerFactory 초기화 및 의존성 주입
        std::cout << "  ⚙️ WorkerFactory 초기화..." << std::endl;
        if (!InitializeWorkerFactory()) {
            std::cout << "  ❌ WorkerFactory 초기화 실패" << std::endl;
            return false;
        }
        std::cout << "  ✅ WorkerFactory 초기화 완료" << std::endl;
        
        std::cout << "✅ 시스템 초기화 완료!" << std::endl;
        logger_->Info("PulseOne Collector v2.0 initialized successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 초기화 오류: " << e.what() << std::endl;
        logger_->Fatal("CollectorApplication::Initialize() failed: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeWorkerFactory() {
    try {
        // WorkerFactory 초기화
        if (!worker_factory_->Initialize()) {
            logger_->Error("Failed to initialize WorkerFactory");
            return false;
        }
        
        // 🔧 Repository들 의존성 주입 (shared_ptr로 수정)
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
        
        // 🔧 데이터베이스 클라이언트들 주입 (메서드명 수정)
        auto redis_client = db_manager_->getRedisClient();     // Ptr 버전 사용
        auto influx_client = db_manager_->getInfluxClient();   // Ptr 버전 사용
        
        worker_factory_->SetDatabaseClients(redis_client, influx_client);
        
        logger_->Info("WorkerFactory initialized and dependencies injected");
        return true;
        
    } catch (const std::exception& e) {
        logger_->Error("InitializeWorkerFactory failed: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::LoadWorkersFromDatabase() {
    try {
        std::cout << "    🔍 데이터베이스에서 모든 활성 디바이스 조회..." << std::endl;
        
        // 🔧 CreateAllActiveWorkers() 메서드 (tenant_id 없는 버전)
        auto created_workers = worker_factory_->CreateAllActiveWorkers();
        
        if (created_workers.empty()) {
            std::cout << "    ⚠️ 활성 디바이스가 없습니다." << std::endl;
            std::cout << "    💡 해결 방법:" << std::endl;
            std::cout << "       1. 데이터베이스에 devices 테이블 데이터 확인" << std::endl;
            std::cout << "       2. devices.is_enabled = 1 인지 확인" << std::endl;
            std::cout << "       3. tenants 테이블에 활성 테넌트 있는지 확인" << std::endl;
            
            logger_->Warn("No active devices found in database");
            return true; // 디바이스가 없어도 시스템은 정상 시작
        }
        
        // 생성된 워커들을 컨테이너에 추가
        for (auto& worker : created_workers) {
            workers_.push_back(std::move(worker));
        }
        
        // 통계 출력
        PrintLoadStatistics();
        
        logger_->Info("Successfully loaded " + std::to_string(workers_.size()) + " workers from database");
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "    ❌ 워커 로드 중 오류: " << e.what() << std::endl;
        logger_->Error("LoadWorkersFromDatabase failed: " + std::string(e.what()));
        return false;
    }
}

void CollectorApplication::PrintLoadStatistics() {
    std::map<uint32_t, std::map<std::string, int>> tenant_stats;
    std::map<std::string, int> protocol_totals;
    
    // 데이터 수집
    for (const auto& worker : workers_) {
        // 🔧 DeviceInfo 타입 수정
        DeviceInfo device_info = worker->GetDeviceInfo();  // PulseOne:: 제거
        
        uint32_t tenant_id = device_info.tenant_id;
        std::string protocol = device_info.protocol_type;
        
        tenant_stats[tenant_id][protocol]++;
        protocol_totals[protocol]++;
    }
    
    std::cout << "    📊 로드 완료 통계:" << std::endl;
    
    // 테넌트별 통계
    std::cout << "    🏢 테넌트별 워커 분포:" << std::endl;
    int total_workers = 0;
    
    for (const auto& [tenant_id, protocols] : tenant_stats) {
        std::cout << "      Tenant " << tenant_id << ":" << std::endl;
        
        int tenant_total = 0;
        for (const auto& [protocol, count] : protocols) {
            std::cout << "        " << protocol << ": " << count << "개" << std::endl;
            tenant_total += count;
        }
        std::cout << "        소계: " << tenant_total << "개" << std::endl;
        total_workers += tenant_total;
    }
    
    // 프로토콜별 통계
    std::cout << "    🔌 프로토콜별 워커 분포:" << std::endl;
    for (const auto& [protocol, count] : protocol_totals) {
        std::cout << "      " << protocol << ": " << count << "개" << std::endl;
    }
    
    std::cout << "    🎯 총 로드된 워커: " << total_workers << "개" << std::endl;
}

void CollectorApplication::StartWorkers() {
    std::cout << "▶️ 워커들 시작 중..." << std::endl;
    
    int started_count = 0;
    int failed_count = 0;
    
    for (auto& worker : workers_) {
        try {
            // 🔧 DeviceInfo 타입 수정
            DeviceInfo device_info = worker->GetDeviceInfo();
            std::cout << "  🔧 " << device_info.name 
                     << " (Tenant:" << device_info.tenant_id 
                     << ", " << device_info.protocol_type << ") 시작 중..." << std::endl;
            
            // BaseDeviceWorker의 Start()가 future<bool>을 반환
            auto start_future = worker->Start();
            bool start_result = start_future.get();
            
            if (start_result) {
                started_count++;
                std::cout << "    ✅ " << device_info.name << " 시작됨" << std::endl;
            } else {
                failed_count++;
                std::cout << "    ❌ " << device_info.name << " 시작 실패" << std::endl;
            }
        } catch (const std::exception& e) {
            failed_count++;
            DeviceInfo device_info = worker->GetDeviceInfo();
            std::cout << "    ❌ " << device_info.name << " 시작 실패: " << e.what() << std::endl;
            logger_->Error("Failed to start worker " + device_info.name + ": " + std::string(e.what()));
        }
    }
    
    std::cout << "📊 워커 시작 결과: ✅ " << started_count << "개 성공, ❌ " << failed_count << "개 실패" << std::endl;
    
    if (failed_count > 0) {
        std::cout << "⚠️ 일부 워커 시작 실패. 로그를 확인하세요." << std::endl;
    }
    
    logger_->Info("Worker startup completed: " + std::to_string(started_count) + " success, " + 
                  std::to_string(failed_count) + " failed");
}

void CollectorApplication::StopWorkers() {
    std::cout << "⏹️ 워커들 중지 중..." << std::endl;
    
    int stopped_count = 0;
    int failed_count = 0;
    
    for (auto& worker : workers_) {
        try {
            DeviceInfo device_info = worker->GetDeviceInfo();
            std::cout << "  🛑 " << device_info.name << " 중지 중..." << std::endl;
            
            // BaseDeviceWorker의 Stop()이 future<bool>을 반환
            auto stop_future = worker->Stop();
            bool stop_result = stop_future.get();
            
            if (stop_result) {
                stopped_count++;
                std::cout << "    ✅ " << device_info.name << " 중지됨" << std::endl;
            } else {
                failed_count++;
                std::cout << "    ❌ " << device_info.name << " 중지 실패" << std::endl;
            }
        } catch (const std::exception& e) {
            failed_count++;
            DeviceInfo device_info = worker->GetDeviceInfo();
            std::cout << "    ❌ " << device_info.name << " 중지 실패: " << e.what() << std::endl;
            logger_->Error("Failed to stop worker " + device_info.name + ": " + std::string(e.what()));
        }
    }
    
    workers_.clear();
    std::cout << "📊 워커 중지 결과: ✅ " << stopped_count << "개 성공, ❌ " << failed_count << "개 실패" << std::endl;
    std::cout << "🛑 모든 워커 중지 완료" << std::endl;
    
    logger_->Info("Worker shutdown completed: " + std::to_string(stopped_count) + " success, " + 
                  std::to_string(failed_count) + " failed");
}

void CollectorApplication::MainLoop() {
    int loop_count = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (is_running_.load()) {
        try {
            // 30초마다 상태 출력
            if (++loop_count % 30 == 0) {
                PrintRuntimeStatistics(loop_count, start_time);
                
                // 5분(300초)마다 상세 통계 출력
                if (loop_count % 300 == 0) {
                    PrintDetailedStatistics();
                }
            }
            
            // 1초 대기
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
        } catch (const std::exception& e) {
            std::cout << "⚠️ 메인 루프 오류: " << e.what() << std::endl;
            logger_->Error("MainLoop error: " + std::string(e.what()));
            // 계속 실행
        }
    }
    
    std::cout << "🔄 메인 루프 종료됨" << std::endl;
}

void CollectorApplication::PrintRuntimeStatistics(int loop_count, 
                                                  const std::chrono::steady_clock::time_point& start_time) {
    
    std::map<std::string, int> state_counts;
    int active_workers = 0;
    int error_workers = 0;
    
    // 워커 상태 통계 수집
    for (const auto& worker : workers_) {
        auto state = worker->GetState();
        // 🔧 WorkerState enum 함수 호출 수정
        std::string state_str = Workers::WorkerStateToString(state);
        state_counts[state_str]++;
        
        if (Workers::IsActiveState(state)) {
            active_workers++;
        } else if (Workers::IsErrorState(state)) {
            error_workers++;
        }
    }
    
    // 실행 시간 계산
    auto now = std::chrono::steady_clock::now();
    auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
    int hours = uptime.count() / 3600;
    int minutes = (uptime.count() % 3600) / 60;
    int seconds = uptime.count() % 60;
    
    std::cout << "💓 시스템 상태 (실행시간: " << hours << "h " << minutes << "m " << seconds << "s)" << std::endl;
    std::cout << "  활성 워커: " << active_workers << "/" << workers_.size();
    
    if (error_workers > 0) {
        std::cout << " (⚠️ 오류: " << error_workers << "개)";
    }
    std::cout << std::endl;
    
    // 상태별 분포 (한 줄로)
    std::cout << "  상태 분포: ";
    for (const auto& [state, count] : state_counts) {
        std::cout << state << "(" << count << ") ";
    }
    std::cout << std::endl;
}

void CollectorApplication::PrintDetailedStatistics() {
    try {
        std::cout << "📈 상세 시스템 통계 (5분 주기):" << std::endl;
        
        // WorkerFactory 통계
        std::cout << "  🏭 WorkerFactory 통계:" << std::endl;
        auto factory_stats = worker_factory_->GetFactoryStatsString();
        std::cout << "    " << factory_stats << std::endl;
        
        // Repository 캐시 통계
        std::cout << "  📊 Repository 캐시 통계:" << std::endl;
        auto cache_stats = repository_factory_->getAllCacheStats();
        for (const auto& [repo_name, stats] : cache_stats) {
            std::cout << "    " << repo_name << ": ";
            for (const auto& [key, value] : stats) {
                std::cout << key << "=" << value << " ";
            }
            std::cout << std::endl;
        }
        
    } catch (const std::exception& e) {
        // 통계 출력 오류는 무시하고 계속 진행
        logger_->Warn("Failed to print detailed statistics: " + std::string(e.what()));
    }
}

void CollectorApplication::Cleanup() {
    if (is_running_.load()) {
        Stop();
    }
    
    try {
        std::cout << "🧹 시스템 정리 중..." << std::endl;
        
        // 1. 워커들 중지
        StopWorkers();
        
        // 2. Repository 팩토리 종료
        if (repository_factory_) {
            repository_factory_->shutdown();
            std::cout << "  ✅ RepositoryFactory 종료 완료" << std::endl;
        }
        
        // 3. 기타 정리 작업
        logger_->Info("CollectorApplication cleanup completed");
        std::cout << "✅ 시스템 정리 완료" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "⚠️ 정리 중 오류: " << e.what() << std::endl;
        logger_->Error("Cleanup error: " + std::string(e.what()));
    }
}

} // namespace Core
} // namespace PulseOne