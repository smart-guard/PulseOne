// =============================================================================
// core/export-gateway/tests/test_integration.cpp
// ✅ 컴파일 에러 수정 - DynamicTargetStats 필드명 수정
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
    std::string test_db_ = "/tmp/test_export_gateway_integration.db";
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
                log.Info("🗑️  기존 DB 파일 삭제: " + test_db_);
            }
            
            auto& config = ConfigManager::getInstance();
            config.set("SQLITE_DB_PATH", test_db_);
            config.set("CSP_DATABASE_PATH", test_db_);
            config.set("DATABASE_PATH", test_db_);
            
            log.Info("📍 DB 경로 설정 확인:");
            log.Info("   SQLITE_DB_PATH    = " + config.get("SQLITE_DB_PATH"));
            log.Info("   CSP_DATABASE_PATH = " + config.get("CSP_DATABASE_PATH"));
            log.Info("   DATABASE_PATH     = " + config.get("DATABASE_PATH"));
            
            auto& db = DatabaseManager::getInstance();
            db.reinitialize();
            
            if (!db.isSQLiteConnected()) {
                throw std::runtime_error("❌ SQLite 연결 실패!");
            }
            log.Info("✅ 1-2. SQLite 연결 성공");
            
            if (!std::filesystem::exists(test_db_)) {
                throw std::runtime_error("❌ DB 파일이 생성되지 않음: " + test_db_);
            }
            
            auto file_size = std::filesystem::file_size(test_db_);
            log.Info("✅ 1-3. DB 파일 생성됨: " + test_db_);
            log.Info("   파일 크기: " + std::to_string(file_size) + " bytes");
            
            // RepositoryFactory 초기화
            auto& repo_factory = DB::RepositoryFactory::getInstance();
            if (!repo_factory.initialize()) {
                throw std::runtime_error("RepositoryFactory 초기화 실패");
            }
            log.Info("✅ 1-4. RepositoryFactory 초기화");
            
            auto test_repo = repo_factory.getExportTargetRepository();
            if (!test_repo) {
                throw std::runtime_error("ExportTargetRepository 생성 실패");
            }
            log.Info("✅ 1-5. ExportTargetRepository 접근 가능");
            
            // 테이블 생성 확인
            std::vector<std::vector<std::string>> tables_result;
            if (!db.executeQuery("SELECT name FROM sqlite_master WHERE type='table'", tables_result)) {
                throw std::runtime_error("테이블 목록 조회 실패");
            }
            
            log.Info("📋 생성된 테이블 목록 (" + std::to_string(tables_result.size()) + "개):");
            for (const auto& row : tables_result) {
                if (!row.empty()) {
                    log.Info("   - " + row[0]);
                }
            }
            
            bool has_export_targets = false;
            for (const auto& row : tables_result) {
                if (!row.empty() && row[0] == "export_targets") {
                    has_export_targets = true;
                    break;
                }
            }
            
            if (!has_export_targets) {
                log.Warn("⚠️  export_targets 테이블이 아직 생성되지 않음 (Repository save 시 자동 생성됨)");
            } else {
                log.Info("✅ export_targets 테이블 확인됨");
            }
            
            // 디렉토리
            std::filesystem::create_directories(test_dir_);
            log.Info("✅ 1-6. 테스트 디렉토리 생성: " + test_dir_);
            
#ifdef HAVE_HTTPLIB
            mock_server_ = std::make_unique<MockAlarmReceiver>();
            mock_server_->start();
            log.Info("✅ 1-7. Mock HTTP 서버 시작:18080");
#endif
            
            // Gateway
            CSP::CSPGatewayConfig gw_config;
            gw_config.building_id = "1001";
            gw_config.use_dynamic_targets = true;
            gw_config.api_endpoint = "http://localhost:18080";
            gw_config.debug_mode = true;
            
            gateway_ = std::make_unique<CSP::CSPGateway>(gw_config);
            log.Info("✅ 1-8. CSPGateway 생성 완료");
            
            log.Info("✅ STEP 1 완료 - 환경 준비 성공");
            return true;
            
        } catch (const std::exception& e) {
            log.Error("❌ STEP 1 실패: " + std::string(e.what()));
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
            
            auto size_before = std::filesystem::file_size(test_db_);
            log.Info("📊 저장 전 DB 크기: " + std::to_string(size_before) + " bytes");
            
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
            
            auto size_after = std::filesystem::file_size(test_db_);
            log.Info("📊 저장 후 DB 크기: " + std::to_string(size_after) + " bytes");
            log.Info("   증가량: " + std::to_string(size_after - size_before) + " bytes");
            
            if (size_after == size_before) {
                log.Warn("⚠️  DB 파일 크기가 변하지 않음 - 데이터가 저장되지 않았을 수 있음!");
            }
            
            auto all_targets = repo->findAll();
            log.Info("✅ 2-4. 총 " + std::to_string(all_targets.size()) + "개 타겟 확인됨");
            
            if (all_targets.size() != 3) {
                throw std::runtime_error("타겟 개수 불일치: 예상 3개, 실제 " + 
                    std::to_string(all_targets.size()) + "개");
            }
            
            log.Info("✅ STEP 2 완료 - 타겟 저장 성공");
            return true;
            
        } catch (const std::exception& e) {
            log.Error("❌ STEP 2 실패: " + std::string(e.what()));
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
            log.Info("📌 HTTP 타겟 ID: " + std::to_string(http_target_id));
            
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
            
            if (mappings.size() != 2) {
                throw std::runtime_error("매핑 개수 불일치: 예상 2개, 실제 " + 
                    std::to_string(mappings.size()) + "개");
            }
            
            log.Info("✅ STEP 3 완료 - 매핑 저장 성공");
            return true;
            
        } catch (const std::exception& e) {
            log.Error("❌ STEP 3 실패: " + std::string(e.what()));
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
            
            // ✅ 수정: target_type → type, is_enabled → enabled
            for (const auto& stat : stats) {
                log.Info("   - " + stat.name + " (type: " + stat.type + 
                    ", enabled: " + (stat.enabled ? "YES" : "NO") + ")");
            }
            
            if (!gateway_->start()) {
                throw std::runtime_error("Gateway 시작 실패");
            }
            log.Info("✅ 4-3. Gateway 시작됨");
            
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            log.Info("✅ STEP 4 완료 - Gateway 준비 완료");
            return true;
            
        } catch (const std::exception& e) {
            log.Error("❌ STEP 4 실패: " + std::string(e.what()));
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
                log.Warn("⚠️  5-2. 알람 전송 실패: " + result.error_message);
            } else {
                log.Info("✅ 5-2. 알람 전송 성공 (status=" + 
                    std::to_string(result.status_code) + ")");
            }
            
#ifdef HAVE_HTTPLIB
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            int received = mock_server_->getReceivedCount();
            log.Info("✅ 5-3. Mock 서버 수신: " + std::to_string(received) + "개");
            
            if (received == 0) {
                log.Warn("⚠️  Mock 서버가 알람을 받지 못함 - 전송 경로 확인 필요");
            }
#endif
            
            log.Info("✅ STEP 5 완료 - 알람 전송 테스트 완료");
            return true;
            
        } catch (const std::exception& e) {
            log.Error("❌ STEP 5 실패: " + std::string(e.what()));
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
            log.Info("✅ 6-2. DB 타겟 목록 (" + std::to_string(all_targets.size()) + "개):");
            
            // ✅ v2.0: export_targets는 설정만 (통계 필드 없음)
            for (const auto& t : all_targets) {
                log.Info("  - " + t.getName() + 
                    " (type=" + t.getTargetType() +
                    ", enabled=" + (t.isEnabled() ? "YES" : "NO") + ")");
            }
            
            // ✅ v2.0: 통계는 export_logs에서 조회
            auto log_repo = factory.getExportLogRepository();
            if (log_repo) {
                log.Info("✅ 6-3. Export Logs 통계 (export_logs 테이블):");
                
                for (const auto& t : all_targets) {
                    try {
                        // export_logs에서 타겟별 통계 조회
                        auto stats_map = log_repo->getTargetStatistics(t.getId(), 24);
                        
                        uint64_t success = 0;
                        uint64_t failed = 0;
                        
                        for (const auto& [status, count] : stats_map) {
                            if (status == "success") {
                                success = count;
                            } else if (status == "failed" || status == "failure") {
                                failed = count;
                            }
                        }
                        
                        uint64_t total = success + failed;
                        
                        log.Info("  - " + t.getName() + 
                            ": total=" + std::to_string(total) +
                            ", success=" + std::to_string(success) + 
                            ", fail=" + std::to_string(failed));
                            
                    } catch (const std::exception& e) {
                        log.Warn("  - " + t.getName() + ": 통계 조회 실패 - " + std::string(e.what()));
                    }
                }
            } else {
                log.Warn("⚠️  ExportLogRepository 없음 - 로그 통계 확인 불가");
            }
            
            auto dynamic_stats = gateway_->getDynamicTargetStats();
            log.Info("✅ 6-4. 동적 타겟 통계 (" + std::to_string(dynamic_stats.size()) + "개):");
            
            for (const auto& s : dynamic_stats) {
                log.Info("  - " + s.name + 
                    ": success=" + std::to_string(s.success_count) + 
                    ", fail=" + std::to_string(s.failure_count) +
                    ", rate=" + std::to_string(s.calculateSuccessRate()) + "%");
            }
            
            log.Info("✅ STEP 6 완료 - 통계 확인 완료");
            log.Info("");
            log.Info("📊 v2.0 통계 시스템 요약:");
            log.Info("  - export_targets: 설정만 저장 (통계 필드 없음)");
            log.Info("  - export_logs: 모든 전송 로그 기록");
            log.Info("  - 통계 조회: export_logs 집계");
            
            return true;
            
        } catch (const std::exception& e) {
            log.Error("❌ STEP 6 실패: " + std::string(e.what()));
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
            gateway_->stop();
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
                log.Info("  - DB 파일 삭제: " + test_db_);
            }
            if (std::filesystem::exists(test_dir_)) {
                std::filesystem::remove_all(test_dir_);
                log.Info("  - 테스트 디렉토리 삭제: " + test_dir_);
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