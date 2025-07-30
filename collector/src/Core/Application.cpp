/**
 * @file Application.cpp
 * @brief PulseOne Collector v2.0 Simple Edition 구현
 * @author PulseOne Development Team
 * @date 2025-07-30
 */

#include "Core/Application.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Workers/Base/BaseDeviceWorker.h"
#include "Workers/Protocol/ModbusTcpWorker.h"
#include "Workers/Protocol/MQTTWorker.h"
#include "Workers/Protocol/BACnetWorker.h"
#include <iostream>
#include <thread>
#include <chrono>

namespace PulseOne {
namespace Core {

CollectorApplication::CollectorApplication() 
    : is_running_(false)
    , logger_(&LogManager::getInstance())
    , config_manager_(&ConfigManager::getInstance())
    , db_manager_(&DatabaseManager::getInstance())
    , repository_factory_(&PulseOne::Database::RepositoryFactory::getInstance()) {
    
    std::cout << "🔧 CollectorApplication 생성됨" << std::endl;
}

CollectorApplication::~CollectorApplication() {
    Cleanup();
    std::cout << "🗑️ CollectorApplication 정리 완료" << std::endl;
}

void CollectorApplication::Run() {
    std::cout << "🚀 PulseOne Collector 시작 중..." << std::endl;
    
    try {
        // 1. 초기화
        if (!Initialize()) {
            std::cout << "❌ 초기화 실패" << std::endl;
            return;
        }
        
        // 2. 워커 시작
        StartWorkers();
        
        // 3. 실행 상태 설정
        is_running_.store(true);
        std::cout << "✅ PulseOne Collector 시작 완료" << std::endl;
        std::cout << "📊 활성 워커: " << workers_.size() << "개" << std::endl;
        std::cout << "🔄 메인 루프 실행 중... (Ctrl+C로 종료)" << std::endl;
        
        // 4. 메인 루프
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
        if (!config_manager_->initialize()) {
            std::cout << "  ❌ ConfigManager 초기화 실패" << std::endl;
            return false;
        }
        std::cout << "  ✅ ConfigManager 초기화 완료" << std::endl;
        
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
        
        // 4. 프로토콜 워커들 초기화
        std::cout << "  ⚙️ Protocol Workers 초기화..." << std::endl;
        if (!InitializeWorkers()) {
            std::cout << "  ❌ Protocol Workers 초기화 실패" << std::endl;
            return false;
        }
        std::cout << "  ✅ Protocol Workers 초기화 완료" << std::endl;
        
        std::cout << "✅ 시스템 초기화 완료!" << std::endl;
        logger_->Info("PulseOne Collector v2.0 initialized successfully");
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 초기화 오류: " << e.what() << std::endl;
        logger_->Fatal("CollectorApplication::Initialize() failed: " + std::string(e.what()));
        return false;
    }
}

bool CollectorApplication::InitializeWorkers() {
    try {
        int initialized_count = 0;
        
        // Modbus TCP 워커 초기화
        if (config_manager_->getOrDefault("MODBUS_TCP_ENABLED", "true") == "true") {
            std::cout << "    🔧 Modbus TCP Worker..." << std::endl;
            
            auto modbus_worker = std::make_unique<PulseOne::Workers::Protocol::ModbusTcpWorker>();
            if (modbus_worker->initialize()) {
                workers_.push_back(std::move(modbus_worker));
                initialized_count++;
                std::cout << "    ✅ Modbus TCP Worker 준비됨" << std::endl;
            } else {
                std::cout << "    ⚠️ Modbus TCP Worker 초기화 실패" << std::endl;
            }
        }
        
        // MQTT 워커 초기화
        if (config_manager_->getOrDefault("MQTT_ENABLED", "true") == "true") {
            std::cout << "    📡 MQTT Worker..." << std::endl;
            
            auto mqtt_worker = std::make_unique<PulseOne::Workers::Protocol::MQTTWorker>();
            if (mqtt_worker->initialize()) {
                workers_.push_back(std::move(mqtt_worker));
                initialized_count++;
                std::cout << "    ✅ MQTT Worker 준비됨" << std::endl;
            } else {
                std::cout << "    ⚠️ MQTT Worker 초기화 실패" << std::endl;
            }
        }
        
        // BACnet 워커 초기화 (선택적)
        if (config_manager_->getOrDefault("BACNET_ENABLED", "false") == "true") {
            std::cout << "    🏢 BACnet Worker..." << std::endl;
            
            auto bacnet_worker = std::make_unique<PulseOne::Workers::Protocol::BACnetWorker>();
            if (bacnet_worker->initialize()) {
                workers_.push_back(std::move(bacnet_worker));
                initialized_count++;
                std::cout << "    ✅ BACnet Worker 준비됨" << std::endl;
            } else {
                std::cout << "    ⚠️ BACnet Worker 초기화 실패" << std::endl;
            }
        }
        
        std::cout << "    📊 초기화된 워커: " << initialized_count << "개" << std::endl;
        logger_->Info("Initialized " + std::to_string(initialized_count) + " protocol workers");
        
        return true;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 워커 초기화 오류: " << e.what() << std::endl;
        logger_->Error("InitializeWorkers() failed: " + std::string(e.what()));
        return false;
    }
}

void CollectorApplication::StartWorkers() {
    std::cout << "▶️ 워커들 시작 중..." << std::endl;
    
    int started_count = 0;
    for (auto& worker : workers_) {
        try {
            worker->start();
            started_count++;
            std::cout << "  ✅ " << worker->getName() << " 시작됨" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  ❌ " << worker->getName() << " 시작 실패: " << e.what() << std::endl;
            logger_->Error("Failed to start worker " + worker->getName() + ": " + std::string(e.what()));
        }
    }
    
    std::cout << "📊 시작된 워커: " << started_count << "/" << workers_.size() << std::endl;
    logger_->Info("Started " + std::to_string(started_count) + " workers");
}

void CollectorApplication::StopWorkers() {
    std::cout << "⏹️ 워커들 중지 중..." << std::endl;
    
    for (auto& worker : workers_) {
        try {
            worker->stop();
            std::cout << "  ✅ " << worker->getName() << " 중지됨" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "  ❌ " << worker->getName() << " 중지 실패: " << e.what() << std::endl;
            logger_->Error("Failed to stop worker " + worker->getName() + ": " + std::string(e.what()));
        }
    }
    
    workers_.clear();
    std::cout << "🛑 모든 워커 중지 완료" << std::endl;
    logger_->Info("All workers stopped");
}

void CollectorApplication::MainLoop() {
    int loop_count = 0;
    
    while (is_running_.load()) {
        try {
            // 30초마다 상태 출력
            if (++loop_count % 30 == 0) {
                int active_workers = 0;
                for (const auto& worker : workers_) {
                    if (worker->isRunning()) {
                        active_workers++;
                    }
                }
                
                std::cout << "💓 활성 워커: " << active_workers << "/" << workers_.size() 
                         << " (실행 시간: " << loop_count << "초)" << std::endl;
                
                // Repository 통계 출력 (5분마다)
                if (loop_count % 300 == 0) {
                    try {
                        auto cache_stats = repository_factory_->getAllCacheStats();
                        std::cout << "📈 캐시 통계:" << std::endl;
                        for (const auto& [repo_name, stats] : cache_stats) {
                            if (stats.find("hits") != stats.end()) {
                                std::cout << "  " << repo_name << ": " 
                                         << stats.at("hits") << " hits" << std::endl;
                            }
                        }
                    } catch (const std::exception& e) {
                        // 캐시 통계 오류는 무시
                    }
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

void CollectorApplication::Cleanup() {
    if (is_running_.load()) {
        Stop();
    }
    
    try {
        // 1. 워커들 중지
        StopWorkers();
        
        // 2. Repository 팩토리 종료
        if (repository_factory_) {
            repository_factory_->shutdown();
            std::cout << "✅ RepositoryFactory 종료 완료" << std::endl;
        }
        
        // 3. 기타 정리 작업
        logger_->Info("CollectorApplication cleanup completed");
        
    } catch (const std::exception& e) {
        std::cout << "⚠️ 정리 중 오류: " << e.what() << std::endl;
    }
}

} // namespace Core
} // namespace PulseOne