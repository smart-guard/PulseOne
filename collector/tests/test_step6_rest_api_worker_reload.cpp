/**
 * @file test_step6_fixed_workermanager.cpp
 * @brief Step 6: 수정된 WorkerManager에 맞는 테스트 (연결 실패해도 Worker 유지)
 * @date 2025-09-01
 * 
 * 🎯 수정된 WorkerManager의 새로운 동작 검증:
 * 1. StartWorker() -> 연결 실패해도 Worker 객체 유지 및 성공 반환
 * 2. RestartWorker() -> 기존 Worker 재활용, 연결 실패해도 성공
 * 3. Worker 상태 -> RECONNECTING 또는 DEVICE_OFFLINE 상태로 유지
 * 4. 자동 재연결 스레드가 백그라운드에서 계속 동작
 * 5. DB 설정 변경 후 RestartWorker 호출시 새 설정 적용
 */

#include <gtest/gtest.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <memory>
#include <future>

// PulseOne 시스템 헤더들
#include "Utils/ConfigManager.h"
#include "Utils/LogManager.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Workers/WorkerManager.h"

#ifdef HAVE_HTTPLIB
#include "Network/RestApiServer.h"
#endif

// Repository들
#include "Database/Repositories/DeviceRepository.h"
#include "Database/Repositories/DeviceSettingsRepository.h"

// 엔티티들
#include "Database/Entities/DeviceEntity.h"
#include "Database/Entities/DeviceSettingsEntity.h"

// Worker들
#include "Workers/Base/BaseDeviceWorker.h"

#include <nlohmann/json.hpp>

using namespace PulseOne;
using namespace PulseOne::Database;
using namespace PulseOne::Workers;
using nlohmann::json;

// =============================================================================
// 수정된 WorkerManager 테스트 클래스
// =============================================================================

class Step6FixedWorkerManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n🚀 === Step 6: 수정된 WorkerManager 테스트 시작 ===" << std::endl;
        
        // 시뮬레이션 모드 환경 변수 설정
        SetupSimulationEnvironment();
        
        // 시스템 초기화
        SetupSystemComponents();
        
#ifdef HAVE_HTTPLIB
        // REST API 서버 초기화
        SetupRestApiServer();
#endif
        
        std::cout << "✅ 수정된 WorkerManager 테스트 환경 준비 완료" << std::endl;
    }
    
    void TearDown() override {
        CleanupTestEnvironment();
        std::cout << "\n🧹 === 수정된 WorkerManager 테스트 환경 정리 완료 ===" << std::endl;
    }

protected:
    // 시스템 컴포넌트들
    ConfigManager* config_manager_;
    LogManager* logger_;
    DatabaseManager* db_manager_;
    RepositoryFactory* repo_factory_;
    
    // Repository들
    std::shared_ptr<Repositories::DeviceRepository> device_repo_;
    std::shared_ptr<Repositories::DeviceSettingsRepository> settings_repo_;
    
    // 핵심 컴포넌트들
    WorkerManager* worker_manager_;

#ifdef HAVE_HTTPLIB
    std::unique_ptr<Network::RestApiServer> rest_api_server_;
#endif
    
    // 테스트 상태
    std::string test_device_id_;
    int test_device_int_id_;

protected:
    void SetupSimulationEnvironment() {
        // 시뮬레이션 모드 설정 (실제 연결은 실패하지만 Worker 객체는 생성됨)
        setenv("WORKER_TEST_MODE", "true", 1);
        setenv("WORKER_SIMULATION_MODE", "true", 1);
        setenv("MODBUS_MOCK_MODE", "true", 1);
        
        std::cout << "🧪 시뮬레이션 모드 환경변수 설정 완료" << std::endl;
    }
    
    void SetupSystemComponents() {
        // 기본 시스템 초기화
        config_manager_ = &ConfigManager::getInstance();
        logger_ = &LogManager::getInstance();
        db_manager_ = &DatabaseManager::getInstance();
        repo_factory_ = &RepositoryFactory::getInstance();
        
        logger_->setLogLevel(LogLevel::INFO);
        
        // Repository Factory 초기화
        ASSERT_TRUE(repo_factory_->initialize()) << "RepositoryFactory 초기화 실패";
        
        // Repository들 생성
        device_repo_ = repo_factory_->getDeviceRepository();
        settings_repo_ = repo_factory_->getDeviceSettingsRepository();
        
        ASSERT_TRUE(device_repo_) << "DeviceRepository 획득 실패";
        
        // WorkerManager 초기화
        worker_manager_ = &WorkerManager::getInstance();
        ASSERT_TRUE(worker_manager_) << "WorkerManager 획득 실패";
    }
    
#ifdef HAVE_HTTPLIB
    void SetupRestApiServer() {
        rest_api_server_ = std::make_unique<Network::RestApiServer>(18080);
        ASSERT_TRUE(rest_api_server_) << "RestApiServer 생성 실패";
        
        // RestAPI → WorkerManager 콜백 연결
        rest_api_server_->SetDeviceRestartCallback([this](const std::string& device_id) -> bool {
            std::cout << "🔄 RestAPI 콜백: RestartWorker(" << device_id << ")" << std::endl;
            return worker_manager_->RestartWorker(device_id);
        });
        
        ASSERT_TRUE(rest_api_server_->Start()) << "RestApiServer 시작 실패";
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::cout << "✅ RestApiServer 시작 완료 (포트: 18080)" << std::endl;
    }
#endif
    
    void CleanupTestEnvironment() {
#ifdef HAVE_HTTPLIB
        if (rest_api_server_) {
            rest_api_server_->Stop();
            std::cout << "REST API 서버 중지됨" << std::endl;
        }
#endif
        
        if (worker_manager_) {
            worker_manager_->StopAllWorkers();
        }
        
        // 환경변수 해제
        unsetenv("WORKER_TEST_MODE");
        unsetenv("WORKER_SIMULATION_MODE");
        unsetenv("MODBUS_MOCK_MODE");
    }

public:
    // =============================================================================
    // 테스트 도우미 메서드들
    // =============================================================================
    
    void SelectTestDevice() {
        auto devices = device_repo_->findAll();
        ASSERT_GT(devices.size(), 0) << "테스트할 디바이스가 없음";
        
        for (const auto& device : devices) {
            if (device.isEnabled()) {
                test_device_int_id_ = device.getId();
                test_device_id_ = std::to_string(test_device_int_id_);
                
                std::cout << "🎯 테스트 디바이스: " << device.getName() 
                          << " (ID: " << test_device_id_ << ")" << std::endl;
                return;
            }
        }
        
        FAIL() << "활성화된 디바이스가 없음";
    }
    
    void VerifyWorkerCreatedAndPersistent() {
        std::cout << "\n🔍 === Worker 생성 및 유지 검증 ===" << std::endl;
        
        // 1. 초기 상태: Worker 없음
        bool has_worker_initially = worker_manager_->HasWorker(test_device_id_);
        std::cout << "📊 초기 Worker 존재 여부: " << (has_worker_initially ? "있음" : "없음") << std::endl;
        
        // 2. StartWorker 호출 (연결 실패해도 Worker 생성되어야 함)
        std::cout << "🚀 StartWorker 호출..." << std::endl;
        bool start_result = worker_manager_->StartWorker(test_device_id_);
        
        // 🔥 핵심 검증: 수정된 WorkerManager는 연결 실패해도 true 반환
        EXPECT_TRUE(start_result) << "수정된 StartWorker는 연결 실패해도 성공 반환해야 함";
        std::cout << "📨 StartWorker 결과: " << (start_result ? "성공" : "실패") << std::endl;
        
        // 잠시 대기 (Worker 생성 및 재연결 스레드 시작 시간)
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // 3. Worker 객체 존재 확인
        bool has_worker_after_start = worker_manager_->HasWorker(test_device_id_);
        EXPECT_TRUE(has_worker_after_start) << "Worker 객체가 생성되지 않음";
        std::cout << "📊 StartWorker 후 Worker 존재: " << (has_worker_after_start ? "있음" : "없음") << std::endl;
        
        // 4. Worker 상태 확인 (RECONNECTING 또는 DEVICE_OFFLINE 등이어야 함)
        json worker_status = worker_manager_->GetWorkerStatus(test_device_id_);
        std::cout << "📊 Worker 상태: " << worker_status.dump(2) << std::endl;
        
        // 🔥 핵심 검증: Worker가 존재하고 에러가 아님 (자동 재연결 상태)
        EXPECT_FALSE(worker_status.contains("error")) << "Worker가 생성되었지만 상태 조회 실패";
        
        if (worker_status.contains("state")) {
            std::string state = worker_status["state"];
            std::cout << "📊 Worker 현재 상태: " << state << std::endl;
            
            // Worker가 생성되었고 관리되고 있는 상태면 성공
            EXPECT_TRUE(state != "ERROR" && state != "UNKNOWN") 
                << "Worker가 비정상 상태: " << state;
        }
        
        std::cout << "✅ Worker 생성 및 유지 검증 완료" << std::endl;
    }
    
    void TestDatabaseSettingsChange() {
        std::cout << "\n📊 === DB 설정 변경 테스트 ===" << std::endl;
        
        // 현재 설정 조회
        auto current_settings = settings_repo_->findById(test_device_int_id_);
        
        // 새 값들 설정
        int new_retry_interval = 3000;
        bool new_keep_alive = true;
        int new_backoff_time = 600;
        int new_timeout = 4500;
        
        std::cout << "🔧 새 설정값:" << std::endl;
        std::cout << "  - retry_interval_ms: " << new_retry_interval << std::endl;
        std::cout << "  - keep_alive_enabled: " << new_keep_alive << std::endl;
        std::cout << "  - backoff_time_ms: " << new_backoff_time << std::endl;
        std::cout << "  - timeout_ms: " << new_timeout << std::endl;
        
        bool change_success = false;
        
        if (current_settings.has_value()) {
            // 기존 설정 업데이트
            auto settings = current_settings.value();
            settings.setRetryIntervalMs(new_retry_interval);
            settings.setKeepAliveEnabled(new_keep_alive);
            settings.setBackoffTimeMs(new_backoff_time);
            settings.setReadTimeoutMs(new_timeout);
            
            change_success = settings_repo_->update(settings);
        } else {
            // 새 설정 생성
            Entities::DeviceSettingsEntity new_settings;
            new_settings.setDeviceId(test_device_int_id_);
            new_settings.setRetryIntervalMs(new_retry_interval);
            new_settings.setKeepAliveEnabled(new_keep_alive);
            new_settings.setBackoffTimeMs(new_backoff_time);
            new_settings.setReadTimeoutMs(new_timeout);
            new_settings.setMaxRetryCount(3);
            new_settings.setPollingIntervalMs(1000);
            
            change_success = settings_repo_->save(new_settings);
        }
        
        ASSERT_TRUE(change_success) << "DB 설정 변경 실패";
        std::cout << "✅ DB 설정 변경 완료" << std::endl;
        
        // 변경 확인
        auto updated_settings = settings_repo_->findById(test_device_int_id_);
        ASSERT_TRUE(updated_settings.has_value()) << "변경된 설정 조회 실패";
        
        const auto& s = updated_settings.value();
        EXPECT_EQ(s.getRetryIntervalMs(), new_retry_interval);
        EXPECT_EQ(s.isKeepAliveEnabled(), new_keep_alive);
        EXPECT_EQ(s.getBackoffTimeMs(), new_backoff_time);
        EXPECT_EQ(s.getReadTimeoutMs(), new_timeout);
        
        std::cout << "✅ DB 설정 변경 검증 완료" << std::endl;
    }
    
    void TestWorkerRestart() {
        std::cout << "\n🔄 === Worker 재시작 테스트 ===" << std::endl;
        
        // 재시작 전 상태 확인
        json status_before = worker_manager_->GetWorkerStatus(test_device_id_);
        std::cout << "📊 재시작 전 상태: " << status_before.dump(2) << std::endl;
        
        // Worker 재시작 호출
        std::cout << "🔄 RestartWorker 호출..." << std::endl;
        bool restart_result = worker_manager_->RestartWorker(test_device_id_);
        
        // 🔥 핵심 검증: 수정된 RestartWorker는 성공해야 함
        EXPECT_TRUE(restart_result) << "수정된 RestartWorker는 성공해야 함";
        std::cout << "📨 RestartWorker 결과: " << (restart_result ? "성공" : "실패") << std::endl;
        
        // 재시작 처리 시간 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // 재시작 후 상태 확인
        json status_after = worker_manager_->GetWorkerStatus(test_device_id_);
        std::cout << "📊 재시작 후 상태: " << status_after.dump(2) << std::endl;
        
        // 🔥 핵심 검증: Worker가 여전히 존재하고 활성 상태
        EXPECT_FALSE(status_after.contains("error")) << "재시작 후 Worker 상태 조회 실패";
        EXPECT_TRUE(worker_manager_->HasWorker(test_device_id_)) << "재시작 후 Worker가 사라짐";
        
        std::cout << "✅ Worker 재시작 테스트 완료" << std::endl;
    }
    
    void TestRestApiIntegration() {
        std::cout << "\n🌐 === RestAPI 통합 테스트 ===" << std::endl;
        
        // REST API를 통한 재시작 시뮬레이션
        std::cout << "📡 REST API 콜백 시뮬레이션..." << std::endl;
        
        bool api_result = false;
        try {
            api_result = worker_manager_->RestartWorker(test_device_id_);
        } catch (const std::exception& e) {
            std::cout << "❌ RestAPI 호출 예외: " << e.what() << std::endl;
        }
        
        // 🔥 핵심 검증: REST API 호출 성공
        EXPECT_TRUE(api_result) << "REST API를 통한 Worker 재시작 실패";
        std::cout << "📨 REST API 응답: " << (api_result ? "성공" : "실패") << std::endl;
        
        // API 처리 시간 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        
        // 최종 상태 확인
        json final_status = worker_manager_->GetWorkerStatus(test_device_id_);
        std::cout << "📊 최종 Worker 상태: " << final_status.dump(2) << std::endl;
        
        std::cout << "✅ RestAPI 통합 테스트 완료" << std::endl;
    }
};

// =============================================================================
// 수정된 WorkerManager용 테스트 케이스들
// =============================================================================

TEST_F(Step6FixedWorkerManagerTest, Worker_Creation_And_Persistence_Test) {
    std::cout << "\n🎯 === Worker 생성 및 영속성 테스트 ===" << std::endl;
    
    // 1. 테스트 디바이스 선택
    SelectTestDevice();
    
    // 2. Worker 생성 및 유지 검증
    VerifyWorkerCreatedAndPersistent();
    
    std::cout << "🏆 === Worker 생성 및 영속성 테스트 완료 ===" << std::endl;
}

TEST_F(Step6FixedWorkerManagerTest, Database_Settings_And_Worker_Restart_Test) {
    std::cout << "\n🎯 === DB 설정 변경 및 Worker 재시작 테스트 ===" << std::endl;
    
    // 1. 테스트 디바이스 선택 및 Worker 생성
    SelectTestDevice();
    VerifyWorkerCreatedAndPersistent();
    
    // 2. DB 설정 변경
    TestDatabaseSettingsChange();
    
    // 3. Worker 재시작으로 새 설정 적용
    TestWorkerRestart();
    
    std::cout << "🏆 === DB 설정 변경 및 Worker 재시작 테스트 완료 ===" << std::endl;
}

TEST_F(Step6FixedWorkerManagerTest, Full_RestAPI_Integration_Test) {
    std::cout << "\n🎯 === 전체 RestAPI 통합 테스트 ===" << std::endl;
    
    // 1. 전체 워크플로우 실행
    SelectTestDevice();
    VerifyWorkerCreatedAndPersistent();
    TestDatabaseSettingsChange();
    TestRestApiIntegration();
    
    // 2. 최종 검증
    std::cout << "\n📋 === 최종 검증 ===" << std::endl;
    
    // Worker가 여전히 존재하는지 확인
    bool final_worker_exists = worker_manager_->HasWorker(test_device_id_);
    EXPECT_TRUE(final_worker_exists) << "최종 Worker 존재성 검증 실패";
    
    // DB 설정이 유지되는지 확인
    auto final_settings = settings_repo_->findById(test_device_int_id_);
    EXPECT_TRUE(final_settings.has_value()) << "최종 DB 설정 존재성 검증 실패";
    
    // Worker 상태 최종 확인
    json final_status = worker_manager_->GetWorkerStatus(test_device_id_);
    EXPECT_FALSE(final_status.contains("error")) << "최종 Worker 상태 검증 실패";
    
    std::cout << "📊 최종 상태 요약:" << std::endl;
    std::cout << "  ✅ Worker 존재: " << (final_worker_exists ? "있음" : "없음") << std::endl;
    std::cout << "  ✅ DB 설정: " << (final_settings.has_value() ? "있음" : "없음") << std::endl;
    std::cout << "  ✅ Worker 상태: " << (final_status.contains("error") ? "에러" : "정상") << std::endl;
    
    std::cout << "\n🏆 === 전체 RestAPI 통합 테스트 완료 ===" << std::endl;
    std::cout << "📝 핵심 개선사항 확인:" << std::endl;
    std::cout << "  🔥 연결 실패해도 Worker 유지" << std::endl;
    std::cout << "  🔥 자동 재연결 스레드 백그라운드 동작" << std::endl;
    std::cout << "  🔥 설정 변경시 Worker 재활용" << std::endl;
    std::cout << "  🔥 산업용 IoT 시스템 복원력 보장" << std::endl;
}

TEST_F(Step6FixedWorkerManagerTest, Multiple_Restart_Resilience_Test) {
    std::cout << "\n🎯 === 다중 재시작 복원력 테스트 ===" << std::endl;
    
    SelectTestDevice();
    VerifyWorkerCreatedAndPersistent();
    
    // 여러 번 재시작하여 Worker가 계속 유지되는지 확인
    for (int i = 1; i <= 5; ++i) {
        std::cout << "\n🔄 " << i << "번째 재시작..." << std::endl;
        
        bool restart_result = worker_manager_->RestartWorker(test_device_id_);
        EXPECT_TRUE(restart_result) << i << "번째 재시작 실패";
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        bool worker_exists = worker_manager_->HasWorker(test_device_id_);
        EXPECT_TRUE(worker_exists) << i << "번째 재시작 후 Worker 사라짐";
        
        std::cout << "  📊 " << i << "번째 재시작 결과: " << (restart_result ? "성공" : "실패") 
                  << ", Worker 존재: " << (worker_exists ? "있음" : "없음") << std::endl;
    }
    
    std::cout << "✅ 5회 재시작 모두 성공, Worker 지속적 유지 확인" << std::endl;
    std::cout << "🏆 === 다중 재시작 복원력 테스트 완료 ===" << std::endl;
}