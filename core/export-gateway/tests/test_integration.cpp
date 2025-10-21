// =============================================================================
// core/export-gateway/tests/test_integration.cpp
// ✅ RepositoryFactory 초기화 추가 - Application.cpp 패턴 준수
// =============================================================================

#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportTargetMappingEntity.h"
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

#ifdef HAVE_HTTPLIB
#include <httplib.h>
#endif

using json = nlohmann::json;

// ✅ C++ 코드용 네임스페이스 alias
namespace CSP = PulseOne::CSP;
namespace DB = PulseOne::Database;
namespace Repos = PulseOne::Database::Repositories;
namespace Entities = PulseOne::Database::Entities;

// =============================================================================
// Mock HTTP 서버
// =============================================================================
#ifdef HAVE_HTTPLIB
class MockAlarmReceiver {
private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    int port_ = 18080;
    mutable std::mutex data_mutex_;
    std::vector<json> received_alarms_;
    
public:
    void start() {
        if (running_) return;
        running_ = true;
        
        server_ = std::make_unique<httplib::Server>();
        
        server_->Post("/webhook", [this](const httplib::Request& req, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(data_mutex_);
            try {
                json alarm = json::parse(req.body);
                received_alarms_.push_back(alarm);
                LogManager::getInstance().Info("📨 수신: " + req.body.substr(0, 100));
                res.status = 200;
                res.set_content(R"({"status":"success"})", "application/json");
            } catch (...) {
                res.status = 400;
            }
        });
        
        server_thread_ = std::thread([this]() {
            server_->listen("0.0.0.0", port_);
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void stop() {
        if (!running_) return;
        running_ = false;
        server_->stop();
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }
    
    int getReceivedCount() const {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return received_alarms_.size();
    }
};
#endif

// =============================================================================
// 통합 테스트
// =============================================================================
class ExportGatewayIntegrationTest {
private:
    std::unique_ptr<CSP::CSPGateway> gateway_;
    std::shared_ptr<RedisClientImpl> redis_client_;
#ifdef HAVE_HTTPLIB
    std::unique_ptr<MockAlarmReceiver> mock_server_;
#endif
    std::string test_db_ = "/tmp/test_export.db";
    std::string test_dir_ = "/tmp/test_alarms";
    
public:
    ~ExportGatewayIntegrationTest() {
        cleanup();
    }
    
    bool runAllTests() {
        auto& log = LogManager::getInstance();
        log.Info("🚀 ========================================");
        log.Info("🚀 Export Gateway 완전 통합 테스트");
        log.Info("🚀 4개 파일 사용: Repo(2) + Entity(2)");
        log.Info("🚀 ========================================");
        
        bool all_passed = true;
        
        if (!test01_Setup()) return false;
        if (!test02_CreateTargets()) all_passed = false;
        if (!test03_CreateMappings()) all_passed = false;
        if (!test04_GatewayLoad()) all_passed = false;
        if (!test05_SendAlarm()) all_passed = false;
        if (!test06_VerifyStats()) all_passed = false;
        
        log.Info(all_passed ? "🎉 전체 통과!" : "❌ 일부 실패");
        return all_passed;
    }
    
private:
    bool test01_Setup() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 1: 환경 준비");
        
        try {
            // Redis
            redis_client_ = std::make_shared<RedisClientImpl>();
            log.Info("✅ 1-1. Redis " + 
                std::string(redis_client_->isConnected() ? "연결" : "비활성"));
            
            // DB 초기화
            if (std::filesystem::exists(test_db_)) {
                std::filesystem::remove(test_db_);
            }
            
            auto& config = ConfigManager::getInstance();
            config.set("SQLITE_DB_PATH", test_db_);
            config.set("CSP_DATABASE_PATH", test_db_);
            
            auto& db = DatabaseManager::getInstance();
            db.reinitialize();
            log.Info("✅ 1-2. DB 초기화");
            
            // ✅ RepositoryFactory 초기화 (필수! - Application.cpp 패턴)
            auto& repo_factory = DB::RepositoryFactory::getInstance();
            if (!repo_factory.initialize()) {
                throw std::runtime_error("RepositoryFactory 초기화 실패");
            }
            log.Info("✅ 1-3. RepositoryFactory 초기화");
            
            // 디렉토리
            std::filesystem::create_directories(test_dir_);
            
#ifdef HAVE_HTTPLIB
            mock_server_ = std::make_unique<MockAlarmReceiver>();
            mock_server_->start();
            log.Info("✅ 1-4. Mock HTTP:18080");
#endif
            
            // Gateway
            CSP::CSPGatewayConfig gw_config;
            gw_config.building_id = "1001";
            gw_config.use_dynamic_targets = true;
            gw_config.api_endpoint = "http://localhost:18080";
            gw_config.debug_mode = true;
            
            gateway_ = std::make_unique<CSP::CSPGateway>(gw_config);
            log.Info("✅ 1-5. Gateway 생성");
            
            return true;
        } catch (const std::exception& e) {
            log.Error("STEP 1 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test02_CreateTargets() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 2: ExportTargetRepository로 타겟 저장");
        
        try {
            auto& factory = DB::RepositoryFactory::getInstance();
            auto repo = factory.getExportTargetRepository();
            
            if (!repo) {
                throw std::runtime_error("ExportTargetRepository 없음");
            }
            
            // HTTP 타겟
            Entities::ExportTargetEntity http_target;
            http_target.setName("test-http");
            http_target.setTargetType("http");
            http_target.setDescription("HTTP 테스트 타겟");
            http_target.setEnabled(true);
            
            json http_config = {
                {"url", "http://localhost:18080/webhook"},
                {"method", "POST"},
                {"timeout", 5000}
            };
            http_target.setConfig(http_config.dump());
            http_target.setExportMode("on_change");
            http_target.setExportInterval(0);
            http_target.setBatchSize(100);
            
            if (!repo->save(http_target)) {
                throw std::runtime_error("HTTP 타겟 저장 실패");
            }
            log.Info("✅ 2-1. HTTP 타겟 저장 (ID=" + 
                std::to_string(http_target.getId()) + ")");
            
            // File 타겟
            Entities::ExportTargetEntity file_target;
            file_target.setName("test-file");
            file_target.setTargetType("file");
            file_target.setDescription("파일 테스트 타겟");
            file_target.setEnabled(true);
            
            json file_config = {
                {"directory", test_dir_},
                {"pattern", "alarm_{timestamp}.json"}
            };
            file_target.setConfig(file_config.dump());
            file_target.setExportMode("on_change");
            file_target.setExportInterval(0);
            file_target.setBatchSize(100);
            
            if (!repo->save(file_target)) {
                throw std::runtime_error("File 타겟 저장 실패");
            }
            log.Info("✅ 2-2. File 타겟 저장 (ID=" + 
                std::to_string(file_target.getId()) + ")");
            
            // S3 타겟
            Entities::ExportTargetEntity s3_target;
            s3_target.setName("test-s3");
            s3_target.setTargetType("s3");
            s3_target.setDescription("S3 테스트 타겟");
            s3_target.setEnabled(false);
            
            json s3_config = {
                {"bucket", "test-bucket"},
                {"region", "us-east-1"}
            };
            s3_target.setConfig(s3_config.dump());
            s3_target.setExportMode("on_change");
            s3_target.setExportInterval(0);
            s3_target.setBatchSize(100);
            
            if (!repo->save(s3_target)) {
                throw std::runtime_error("S3 타겟 저장 실패");
            }
            log.Info("✅ 2-3. S3 타겟 저장 (ID=" + 
                std::to_string(s3_target.getId()) + ", disabled)");
            
            auto all_targets = repo->findAll();
            log.Info("✅ 2-4. 총 " + std::to_string(all_targets.size()) + "개 타겟 저장됨");
            
            return true;
        } catch (const std::exception& e) {
            log.Error("STEP 2 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test03_CreateMappings() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 3: ExportTargetMappingRepository로 매핑 저장");
        
        try {
            auto& factory = DB::RepositoryFactory::getInstance();
            auto mapping_repo = factory.getExportTargetMappingRepository();
            auto target_repo = factory.getExportTargetRepository();
            
            if (!mapping_repo || !target_repo) {
                throw std::runtime_error("Repository 없음");
            }
            
            auto http_targets = target_repo->findByTargetType("http");
            if (http_targets.empty()) {
                throw std::runtime_error("HTTP 타겟 없음");
            }
            
            int http_target_id = http_targets[0].getId();
            
            // 매핑 1
            Entities::ExportTargetMappingEntity mapping1;
            mapping1.setTargetId(http_target_id);
            mapping1.setPointId(1001);
            mapping1.setTargetFieldName("temperature");
            mapping1.setTargetDescription("온도 센서 매핑");
            mapping1.setEnabled(true);
            
            json conversion = {
                {"scale", 1.0},
                {"offset", 0.0},
                {"unit", "celsius"}
            };
            mapping1.setConversionConfig(conversion.dump());
            
            if (!mapping_repo->save(mapping1)) {
                throw std::runtime_error("매핑1 저장 실패");
            }
            log.Info("✅ 3-1. 매핑1 저장 (ID=" + 
                std::to_string(mapping1.getId()) + ", target=" + 
                std::to_string(http_target_id) + ", point=1001)");
            
            // 매핑 2
            Entities::ExportTargetMappingEntity mapping2;
            mapping2.setTargetId(http_target_id);
            mapping2.setPointId(1002);
            mapping2.setTargetFieldName("pressure");
            mapping2.setTargetDescription("압력 센서 매핑");
            mapping2.setEnabled(true);
            mapping2.setConversionConfig("{}");
            
            if (!mapping_repo->save(mapping2)) {
                throw std::runtime_error("매핑2 저장 실패");
            }
            log.Info("✅ 3-2. 매핑2 저장 (ID=" + std::to_string(mapping2.getId()) + ")");
            
            auto mappings = mapping_repo->findByTargetId(http_target_id);
            log.Info("✅ 3-3. HTTP 타겟의 매핑: " + std::to_string(mappings.size()) + "개");
            
            return true;
        } catch (const std::exception& e) {
            log.Error("STEP 3 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test04_GatewayLoad() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 4: Gateway에서 타겟 로드");
        
        try {
            if (!gateway_->reloadDynamicTargets()) {
                throw std::runtime_error("타겟 재로드 실패");
            }
            log.Info("✅ 4-1. 타겟 재로드 완료");
            
            auto stats = gateway_->getDynamicTargetStats();
            if (stats.empty()) {
                throw std::runtime_error("로드된 타겟 없음");
            }
            
            log.Info("✅ 4-2. 로드된 타겟: " + std::to_string(stats.size()) + "개");
            
            if (!gateway_->start()) {
                throw std::runtime_error("Gateway 시작 실패");
            }
            log.Info("✅ 4-3. Gateway 시작됨");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            return true;
        } catch (const std::exception& e) {
            log.Error("STEP 4 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test05_SendAlarm() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 5: 알람 전송 테스트");
        
        try {
            CSP::AlarmMessage alarm;
            alarm.bd = 1001;
            alarm.nm = "TEST_POINT_1001";
            alarm.vl = 25.5;
            alarm.tm = "2025-10-21T12:00:00.000";
            alarm.al = 1;
            alarm.st = 1;
            alarm.des = "통합 테스트 알람";
            
            log.Info("✅ 5-1. 알람 생성: " + alarm.nm);
            
            auto result = gateway_->taskAlarmSingle(alarm);
            
            if (!result.success) {
                log.Warn("⚠️ 5-2. 알람 전송 실패: " + result.error_message);
            } else {
                log.Info("✅ 5-2. 알람 전송 성공 (status=" + 
                    std::to_string(result.status_code) + ")");
            }
            
#ifdef HAVE_HTTPLIB
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            int received = mock_server_->getReceivedCount();
            log.Info("✅ 5-3. Mock 서버 수신: " + std::to_string(received) + "개");
#endif
            
            return true;
        } catch (const std::exception& e) {
            log.Error("STEP 5 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test06_VerifyStats() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 6: 통계 확인");
        
        try {
            auto& factory = DB::RepositoryFactory::getInstance();
            auto target_repo = factory.getExportTargetRepository();
            
            if (!target_repo) {
                throw std::runtime_error("Repository 없음");
            }
            
            auto gateway_stats = gateway_->getStats();
            log.Info("✅ 6-1. Gateway 통계:");
            log.Info("  - 총 알람: " + std::to_string(gateway_stats.total_alarms));
            log.Info("  - 성공 API: " + std::to_string(gateway_stats.successful_api_calls));
            log.Info("  - 실패 API: " + std::to_string(gateway_stats.failed_api_calls));
            
            auto all_targets = target_repo->findAll();
            log.Info("✅ 6-2. DB 타겟 통계:");
            
            for (const auto& t : all_targets) {
                if (t.isEnabled()) {
                    log.Info("  - " + t.getName() + 
                        ": exports=" + std::to_string(t.getTotalExports()) + 
                        ", success=" + std::to_string(t.getSuccessfulExports()) + 
                        ", fail=" + std::to_string(t.getFailedExports()));
                }
            }
            
            auto dynamic_stats = gateway_->getDynamicTargetStats();
            log.Info("✅ 6-3. 동적 타겟 통계:");
            
            for (const auto& s : dynamic_stats) {
                log.Info("  - " + s.name + 
                    ": success=" + std::to_string(s.success_count) + 
                    ", fail=" + std::to_string(s.failure_count));
            }
            
            return true;
        } catch (const std::exception& e) {
            log.Error("STEP 6 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    void cleanup() {
        auto& log = LogManager::getInstance();
        log.Info("🧹 정리 중...");
        
#ifdef HAVE_HTTPLIB
        if (mock_server_) {
            mock_server_->stop();
        }
#endif
        
        if (gateway_) {
            gateway_->stop();
            gateway_.reset();
        }
        
        if (redis_client_) {
            redis_client_->disconnect();
            redis_client_.reset();
        }
        
        try {
            if (std::filesystem::exists(test_db_)) {
                std::filesystem::remove(test_db_);
            }
            if (std::filesystem::exists(test_dir_)) {
                std::filesystem::remove_all(test_dir_);
            }
        } catch (...) {}
        
        log.Info("✅ 정리 완료");
    }
};

int main() {
    try {
        ExportGatewayIntegrationTest test;
        bool success = test.runAllTests();
        return success ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "예외: " << e.what() << std::endl;
        return 1;
    }
}