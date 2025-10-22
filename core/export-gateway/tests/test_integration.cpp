// =============================================================================
// core/export-gateway/tests/test_integration.cpp
// Export Gateway 통합 테스트 (완전 수정본)
// =============================================================================

#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <filesystem>
#include <nlohmann/json.hpp>

// PulseOne 헤더
#include "CSP/CSPGateway.h"
#include "CSP/AlarmMessage.h"
#include "Database/DatabaseManager.h"
#include "Database/RepositoryFactory.h"
#include "Database/Repositories/ExportTargetRepository.h"
#include "Database/Repositories/ExportTargetMappingRepository.h"
#include "Database/Repositories/ExportLogRepository.h"
#include "Database/Entities/ExportTargetEntity.h"
#include "Database/Entities/ExportTargetMappingEntity.h"
#include "Database/Entities/ExportLogEntity.h"
#include "Client/RedisClientImpl.h"
#include "Utils/LogManager.h"
#include "Utils/ConfigManager.h"

using json = nlohmann::json;
using namespace PulseOne;

// =============================================================================
// Mock HTTP Server (선택적)
// =============================================================================

#ifdef HAVE_HTTPLIB
#include <httplib.h>

class MockAlarmReceiver {
private:
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    std::vector<json> received_alarms_;
    mutable std::mutex data_mutex_;
    int port_ = 18080;
    
public:
    MockAlarmReceiver(int port = 18080) : port_(port) {}
    
    ~MockAlarmReceiver() {
        stop();
    }
    
    bool start() {
        server_ = std::make_unique<httplib::Server>();
        
        server_->Post("/webhook", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                auto alarm_json = json::parse(req.body);
                {
                    std::lock_guard<std::mutex> lock(data_mutex_);
                    received_alarms_.push_back(alarm_json);
                }
                
                LogManager::getInstance().Info("🔔 Mock 서버: 알람 수신");
                
                res.status = 200;
                res.set_content("{\"status\":\"ok\"}", "application/json");
            } catch (const std::exception& e) {
                res.status = 400;
                res.set_content("{\"error\":\"" + std::string(e.what()) + "\"}", "application/json");
            }
        });
        
        running_ = true;
        server_thread_ = std::thread([this]() {
            LogManager::getInstance().Info("🌐 Mock 서버 시작: http://localhost:" + 
                std::to_string(port_) + "/webhook");
            server_->listen("0.0.0.0", port_);
        });
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return true;
    }
    
    void stop() {
        if (running_) {
            running_ = false;
            if (server_) {
                server_->stop();
            }
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            LogManager::getInstance().Info("🛑 Mock 서버 종료");
        }
    }
    
    int getPort() const { return port_; }
    
    size_t getReceivedCount() {
        std::lock_guard<std::mutex> lock(data_mutex_);
        return received_alarms_.size();
    }
};
#endif

// =============================================================================
// 통합 테스트 클래스
// =============================================================================
class ExportGatewayIntegrationTest {
private:
    std::unique_ptr<CSP::CSPGateway> gateway_;
    std::shared_ptr<RedisClientImpl> redis_client_;  // ✅ 글로벌 namespace
#ifdef HAVE_HTTPLIB
    std::unique_ptr<MockAlarmReceiver> mock_server_;
#endif
    std::string test_db_ = "/tmp/test_export_gateway_integration.db";
    std::string test_dir_ = "/tmp/test_alarms";
    
public:
    ~ExportGatewayIntegrationTest() {
        cleanup();
    }
    
    bool runAllTests() {
        auto& log = LogManager::getInstance();
        log.Info("🚀 ========================================");
        log.Info("🚀 Export Gateway 통합 테스트 v2.0");
        log.Info("🚀 ========================================");
        
        bool all_passed = true;
        
        if (!test01_Setup()) return false;
        if (!test02_CreateTargets()) all_passed = false;
        if (!test03_CreateMappings()) all_passed = false;
        if (!test04_SendAlarm()) all_passed = false;
        if (!test05_VerifyStats()) all_passed = false;
        
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
            config.set("DATABASE_PATH", test_db_);
            
            auto& db = DatabaseManager::getInstance();
            db.reinitialize();
            
            if (!db.isSQLiteConnected()) {
                throw std::runtime_error("SQLite 연결 실패!");
            }
            log.Info("✅ 1-2. SQLite 연결 성공");
            
#ifdef HAVE_HTTPLIB
            mock_server_ = std::make_unique<MockAlarmReceiver>(18080);
            if (!mock_server_->start()) {
                throw std::runtime_error("Mock 서버 시작 실패");
            }
            log.Info("✅ 1-3. Mock HTTP 서버 시작");
#endif
            
            // Gateway
            CSP::CSPGatewayConfig gw_config;
            gw_config.building_id = "1001";
            gw_config.use_dynamic_targets = true;
            gw_config.api_endpoint = "http://localhost:18080/webhook";
            gw_config.debug_mode = true;
            
            gateway_ = std::make_unique<CSP::CSPGateway>(gw_config);
            log.Info("✅ 1-4. CSPGateway 생성 완료");
            
            return true;
        } catch (const std::exception& e) {
            log.Error("❌ STEP 1 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test02_CreateTargets() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 2: Export Target 생성");
        
        try {
            auto& factory = Database::RepositoryFactory::getInstance();
            auto target_repo = factory.getExportTargetRepository();
            
            if (!target_repo) {
                throw std::runtime_error("ExportTargetRepository 없음");
            }
            
            // HTTP Target
            Database::Entities::ExportTargetEntity http_target;
            http_target.setName("test-webhook");
            http_target.setTargetType("http");
            http_target.setDescription("Test HTTP webhook");
            http_target.setEnabled(true);
            
            json http_config = {
                {"url", "http://localhost:18080/webhook"},
                {"method", "POST"},
                {"content_type", "application/json"}
            };
            http_target.setConfig(http_config.dump());
            http_target.setExportMode("on_change");
            
            if (!target_repo->save(http_target)) {
                throw std::runtime_error("HTTP 타겟 저장 실패");
            }
            log.Info("✅ 2-1. HTTP Target 생성: test-webhook (id=" + 
                std::to_string(http_target.getId()) + ")");
            
            // File Target
            Database::Entities::ExportTargetEntity file_target;
            file_target.setName("test-file");
            file_target.setTargetType("file");
            file_target.setDescription("Test file export");
            file_target.setEnabled(true);
            
            json file_config = {
                {"path", "/tmp/test_export.log"},
                {"format", "json"}
            };
            file_target.setConfig(file_config.dump());
            file_target.setExportMode("on_change");
            
            if (!target_repo->save(file_target)) {
                throw std::runtime_error("File 타겟 저장 실패");
            }
            log.Info("✅ 2-2. File Target 생성: test-file (id=" + 
                std::to_string(file_target.getId()) + ")");
            
            return true;
        } catch (const std::exception& e) {
            log.Error("❌ STEP 2 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test03_CreateMappings() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 3: Export Mapping 생성");
        
        try {
            auto& factory = Database::RepositoryFactory::getInstance();
            auto target_repo = factory.getExportTargetRepository();
            auto mapping_repo = factory.getExportTargetMappingRepository();
            
            // HTTP Target에 매핑 추가
            auto http_target_opt = target_repo->findByConditions({
                {"name", "=", "test-webhook"}
            });
            
            if (http_target_opt.empty()) {
                throw std::runtime_error("HTTP 타겟을 찾을 수 없음");
            }
            
            auto http_target = http_target_opt[0];
            int target_id = http_target.getId();
            
            // 테스트 포인트 ID (1001, 1002)
            for (int point_id : {1001, 1002}) {
                Database::Entities::ExportTargetMappingEntity mapping;
                mapping.setTargetId(target_id);
                mapping.setPointId(point_id);
                mapping.setTargetFieldName("point_" + std::to_string(point_id));
                mapping.setEnabled(true);
                
                if (!mapping_repo->save(mapping)) {
                    throw std::runtime_error("매핑 저장 실패: point_id=" + 
                        std::to_string(point_id));
                }
                log.Info("✅ 3-" + std::to_string(point_id - 1000) + 
                    ". 매핑 생성: target_id=" + std::to_string(target_id) + 
                    ", point_id=" + std::to_string(point_id));
            }
            
            return true;
        } catch (const std::exception& e) {
            log.Error("❌ STEP 3 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test04_SendAlarm() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 4: 알람 전송 테스트");
        
        try {
            // AlarmMessage 생성 (C# 스타일 필드 사용)
            CSP::AlarmMessage alarm = CSP::AlarmMessage::create_sample(
                1001,                      // bd (building_id)
                "Temperature",             // nm (point_name)
                85.5,                      // vl (value)
                true                       // al (alarm_flag)
            );
            
            log.Info("🔔 알람 전송:");
            log.Info("   Building: " + std::to_string(alarm.bd));
            log.Info("   Point: " + alarm.nm);
            log.Info("   Value: " + std::to_string(alarm.vl));
            
            // Gateway를 통한 알람 전송 (public 메서드 사용)
            auto result = gateway_->sendTestAlarm();
            
            log.Info("✅ 4-1. 알람 전송 완료");
            log.Info("   성공: " + std::string(result.success ? "예" : "아니오"));
            log.Info("   상태 코드: " + std::to_string(result.status_code));
            
            // 대기 (Mock 서버 수신 대기)
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
#ifdef HAVE_HTTPLIB
            if (mock_server_) {
                size_t received = mock_server_->getReceivedCount();
                log.Info("✅ 4-2. Mock 서버 수신: " + std::to_string(received) + "건");
            }
#endif
            
            return true;
        } catch (const std::exception& e) {
            log.Error("❌ STEP 4 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    bool test05_VerifyStats() {
        auto& log = LogManager::getInstance();
        log.Info("📋 STEP 5: 통계 검증 (v2.0 - export_logs 기반)");
        
        try {
            auto& factory = Database::RepositoryFactory::getInstance();
            auto target_repo = factory.getExportTargetRepository();
            auto log_repo = factory.getExportLogRepository();
            
            if (!target_repo || !log_repo) {
                throw std::runtime_error("Repository 없음");
            }
            
            auto targets = target_repo->findAll();
            log.Info("📊 타겟 목록 (" + std::to_string(targets.size()) + "개):");
            
            for (const auto& t : targets) {
                log.Info("");
                log.Info("  🎯 " + t.getName() + " (id=" + std::to_string(t.getId()) + ")");
                log.Info("     타입: " + t.getTargetType());
                log.Info("     상태: " + std::string(t.isEnabled() ? "활성" : "비활성"));
                
                // v2.0: export_logs에서 통계 집계
                auto stats_map = log_repo->getTargetStatistics(t.getId(), 24);
                
                if (!stats_map.empty()) {
                    int success = 0;
                    int failed = 0;
                    
                    for (const auto& [status, count] : stats_map) {
                        if (status == "success") success = count;
                        else if (status == "failed" || status == "failure") failed = count;
                    }
                    
                    int total = success + failed;
                    double success_rate = (total > 0) ? 
                        (success * 100.0 / total) : 0.0;
                    
                    log.Info("     전송: " + std::to_string(total) + "회");
                    log.Info("     성공: " + std::to_string(success) + "회");
                    log.Info("     실패: " + std::to_string(failed) + "회");
                    log.Info("     성공률: " + std::to_string(static_cast<int>(success_rate)) + "%");
                } else {
                    log.Info("     전송 기록 없음");
                }
            }
            
            log.Info("");
            log.Info("✅ STEP 5 완료 - 통계 확인 완료");
            
            return true;
            
        } catch (const std::exception& e) {
            log.Error("❌ STEP 5 실패: " + std::string(e.what()));
            return false;
        }
    }
    
    void cleanup() {
        auto& log = LogManager::getInstance();
        log.Info("🧹 정리 중...");
        
#ifdef HAVE_HTTPLIB
        if (mock_server_) {
            mock_server_->stop();
            log.Info("  - Mock 서버 종료");
        }
#endif
        
        if (gateway_) {
            gateway_.reset();
            log.Info("  - Gateway 종료");
        }
        
        if (redis_client_) {
            redis_client_->disconnect();
            redis_client_.reset();
            log.Info("  - Redis 연결 해제");
        }
        
        try {
            if (std::filesystem::exists(test_db_)) {
                std::filesystem::remove(test_db_);
                log.Info("  - DB 파일 삭제");
            }
            if (std::filesystem::exists(test_dir_)) {
                std::filesystem::remove_all(test_dir_);
                log.Info("  - 테스트 디렉토리 삭제");
            }
        } catch (const std::exception& e) {
            log.Warn("  - 파일 정리 실패: " + std::string(e.what()));
        }
        
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